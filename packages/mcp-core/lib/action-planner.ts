import { routeTools, type RankedTool, type RoutableTool } from './tool-router.js';

export interface ActionPlannerTool extends RoutableTool {
  inputSchema?: unknown;
}

export type ActionPlanConfidence = 'high' | 'medium' | 'low';

export interface ActionPlannerOptions {
  maxCandidates?: number;
  minScore?: number;
}

export interface ActionPlan<T extends ActionPlannerTool> {
  query: string;
  selected: RankedTool<T> | null;
  candidates: RankedTool<T>[];
  arguments: Record<string, unknown>;
  draftedFields: string[];
  missingRequired: string[];
  confidence: ActionPlanConfidence;
  requiresReview: true;
}

type SchemaRecord = Record<string, unknown>;

const asRecord = (value: unknown): SchemaRecord | null => {
  if (!value || typeof value !== 'object' || Array.isArray(value)) return null;
  return value as SchemaRecord;
};

const schemaForTool = (tool: ActionPlannerTool): SchemaRecord => {
  const direct = asRecord(tool.inputSchema);
  if (direct) return direct;
  if (!tool.schema) return {};

  try {
    return asRecord(JSON.parse(tool.schema)) ?? {};
  } catch {
    return {};
  }
};

const canonicalKey = (value: string): string => value.toLowerCase().replace(/[^a-z0-9]/g, '');

const firstQuotedValue = (query: string): string | null => {
  const patterns = [/"([^"\n]{1,500})"/, /'([^'\n]{1,500})'/, /`([^`\n]{1,500})`/, /“([^”\n]{1,500})”/];
  for (const pattern of patterns) {
    const match = query.match(pattern);
    if (match?.[1]?.trim()) return match[1].trim();
  }
  return null;
};

const extractUrl = (query: string): string | null => query.match(/https?:\/\/[^\s,;)]+/i)?.[0] ?? null;

const extractEmail = (query: string): string | null =>
  query.match(/[A-Z0-9._%+-]+@[A-Z0-9.-]+\.[A-Z]{2,}/i)?.[0] ?? null;

const extractGithubSlug = (query: string): { owner: string; repo: string } | null => {
  const urlMatch = query.match(/github\.com\/([A-Za-z0-9_.-]+)\/([A-Za-z0-9_.-]+)/i);
  if (urlMatch?.[1] && urlMatch[2]) return { owner: urlMatch[1], repo: urlMatch[2].replace(/\.git$/i, '') };

  if (!/(github|repo|repository|仓库|代码库)/i.test(query)) return null;
  const slugMatch = query.match(/\b([A-Za-z0-9_.-]+)\/([A-Za-z0-9_.-]+)\b/);
  if (!slugMatch?.[1] || !slugMatch[2]) return null;
  return { owner: slugMatch[1], repo: slugMatch[2].replace(/\.git$/i, '') };
};

const extractIssueNumber = (query: string): number | null => {
  const match = query.match(/(?:#|issue\s*#?|pr\s*#?|pull\s+request\s*#?|问题\s*#?|工单\s*#?)(\d+)/i);
  if (!match?.[1]) return null;
  const value = Number(match[1]);
  return Number.isFinite(value) ? value : null;
};

const extractLabeledValue = (query: string, labels: string[]): string | null => {
  const lower = query.toLowerCase();
  for (const label of labels) {
    const index = lower.indexOf(label.toLowerCase());
    if (index < 0) continue;

    let tail = query.slice(index + label.length).trimStart();
    tail = tail.replace(/^(?:[:=]|is\b|是|为)\s*/i, '');
    if (!tail) continue;

    const quote = tail[0];
    if (quote === '"' || quote === "'" || quote === '`' || quote === '“') {
      const closing = quote === '“' ? '”' : quote;
      const end = tail.indexOf(closing, 1);
      if (end > 1) return tail.slice(1, end).trim();
    }

    const value = tail
      .split(/[;,\n]/, 1)[0]
      ?.split(/\s+(?:and|with|然后|并且)\s+/i, 1)[0]
      ?.trim();
    if (value) return value;
  }
  return null;
};

const cleanSearchPayload = (query: string): string => {
  const quoted = firstQuotedValue(query);
  if (quoted) return quoted;

  return query
    .replace(/^(?:please\s+)?(?:search|find|look\s+up|query|搜索|查找|检索|搜一下|找一下)\s+(?:for\s+)?/i, '')
    .replace(/\s+/g, ' ')
    .trim();
};

const enumValueFromQuery = (schema: SchemaRecord, query: string): unknown => {
  const values = Array.isArray(schema.enum) ? schema.enum : [];
  const lower = query.toLowerCase();
  for (const value of values) {
    if (typeof value === 'string' && lower.includes(value.toLowerCase())) return value;
    if ((typeof value === 'number' || typeof value === 'boolean') && lower.includes(String(value))) return value;
  }
  return undefined;
};

const inferStringValue = (name: string, query: string): string | null => {
  const key = canonicalKey(name);
  const github = extractGithubSlug(query);

  if (key === 'owner' || key === 'org' || key === 'organization') {
    return github?.owner ?? extractLabeledValue(query, ['owner', 'org', 'organization', '组织', '所有者']);
  }
  if (key === 'repo' || key === 'repository' || key === 'reponame') {
    return github?.repo ?? extractLabeledValue(query, ['repo', 'repository', '仓库', '代码库']);
  }
  if (key.includes('url') || key === 'link' || key === 'href') return extractUrl(query);
  if (key.includes('email') || key === 'to' || key.includes('recipient')) return extractEmail(query);
  if (key.includes('branch') || key === 'ref' || key === 'base' || key === 'head') {
    return extractLabeledValue(query, ['branch', 'ref', 'base', 'head', '分支']);
  }
  if (key.includes('path') || key === 'file' || key === 'filename') {
    return extractLabeledValue(query, [name, 'path', 'file', '文件', '路径']) ?? firstQuotedValue(query);
  }
  if (key === 'query' || key.includes('search') || key === 'q' || key.includes('keyword')) {
    return cleanSearchPayload(query) || null;
  }
  if (key === 'title' || key === 'subject') {
    return extractLabeledValue(query, [name, key, key === 'subject' ? '主题' : '标题']) ?? firstQuotedValue(query);
  }
  if (key === 'name') {
    return extractLabeledValue(query, ['name', '名称', '名字']);
  }
  if (
    key.includes('body') ||
    key.includes('message') ||
    key.includes('content') ||
    key === 'text' ||
    key.includes('comment') ||
    key.includes('description')
  ) {
    return (
      extractLabeledValue(query, [name, 'message', 'body', 'content', 'comment', '消息', '正文', '内容', '评论']) ??
      firstQuotedValue(query)
    );
  }

  return extractLabeledValue(query, [name]);
};

const inferNumberValue = (name: string, query: string): number | null => {
  const key = canonicalKey(name);
  if (
    key.includes('issue') ||
    key.includes('pull') ||
    key.includes('pr') ||
    key.includes('number') ||
    key.endsWith('id')
  ) {
    const issue = extractIssueNumber(query);
    if (issue !== null) return issue;
  }

  const labeled = extractLabeledValue(query, [name]);
  if (!labeled) return null;
  const numeric = Number(labeled.match(/-?\d+(?:\.\d+)?/)?.[0]);
  return Number.isFinite(numeric) ? numeric : null;
};

const inferBooleanValue = (name: string, query: string): boolean | null => {
  const key = name.toLowerCase();
  const lower = query.toLowerCase();
  const index = lower.indexOf(key);
  if (index < 0) return null;
  const tail = lower.slice(index, index + key.length + 40);
  if (/(true|yes|enable|enabled|on|是|开启|启用)/i.test(tail)) return true;
  if (/(false|no|disable|disabled|off|否|关闭|禁用)/i.test(tail)) return false;
  return null;
};

const inferValue = (name: string, schema: SchemaRecord, query: string): unknown => {
  if ('const' in schema) return schema.const;
  if ('default' in schema) return schema.default;

  const enumValue = enumValueFromQuery(schema, query);
  if (enumValue !== undefined) return enumValue;

  const type = typeof schema.type === 'string' ? schema.type : 'string';
  if (type === 'integer' || type === 'number') {
    const value = inferNumberValue(name, query);
    if (value === null) return undefined;
    return type === 'integer' ? Math.trunc(value) : value;
  }
  if (type === 'boolean') return inferBooleanValue(name, query) ?? undefined;
  if (type === 'string') return inferStringValue(name, query) ?? undefined;
  return undefined;
};

const draftArguments = (
  tool: ActionPlannerTool,
  query: string,
): { arguments: Record<string, unknown>; draftedFields: string[]; missingRequired: string[] } => {
  const schema = schemaForTool(tool);
  const properties = asRecord(schema.properties) ?? {};
  const required = Array.isArray(schema.required)
    ? schema.required.filter((value): value is string => typeof value === 'string')
    : [];
  const args: Record<string, unknown> = {};
  const draftedFields: string[] = [];

  Object.entries(properties).forEach(([name, rawProperty]) => {
    const property = asRecord(rawProperty);
    if (!property) return;
    const value = inferValue(name, property, query);
    if (value === undefined || value === null || value === '') return;
    args[name] = value;
    draftedFields.push(name);
  });

  return {
    arguments: args,
    draftedFields,
    missingRequired: required.filter(name => !(name in args)),
  };
};

const confidenceFor = <T extends ActionPlannerTool>(
  selected: RankedTool<T>,
  candidates: RankedTool<T>[],
  missingRequired: string[],
): ActionPlanConfidence => {
  const nextScore = candidates[1]?.score ?? 0;
  const gap = selected.score - nextScore;
  let confidence: ActionPlanConfidence =
    selected.score >= 48 && gap >= 8 ? 'high' : selected.score >= 14 ? 'medium' : 'low';

  if (missingRequired.length > 0 && confidence === 'high') confidence = 'medium';
  if (missingRequired.length >= 2) confidence = 'low';
  return confidence;
};

/**
 * Deterministic zero-model action planner used before Desktop review.
 *
 * Tool selection reuses the shared Tool Router. Argument drafting is intentionally
 * conservative: it only fills values that can be derived from schema defaults or
 * explicit cues in the user's text. It never executes the action; callers must
 * present the draft for review and still use the normal guarded execution path.
 */
export const planAction = <T extends ActionPlannerTool>(
  tools: T[],
  query: string,
  options: ActionPlannerOptions = {},
): ActionPlan<T> => {
  const trimmed = query.trim();
  const routed = routeTools(tools, trimmed, {
    maxTools: Math.max(1, options.maxCandidates ?? 5),
    minScore: options.minScore ?? 1,
  });

  if (!trimmed || !routed.queryUsed || routed.ranked.length === 0 || routed.ranked[0].score <= 0) {
    return {
      query: trimmed,
      selected: null,
      candidates: [],
      arguments: {},
      draftedFields: [],
      missingRequired: [],
      confidence: 'low',
      requiresReview: true,
    };
  }

  const selected = routed.ranked[0];
  const candidates = routed.ranked.slice(0, Math.max(1, options.maxCandidates ?? 5));
  const drafted = draftArguments(selected.tool, trimmed);

  return {
    query: trimmed,
    selected,
    candidates,
    arguments: drafted.arguments,
    draftedFields: drafted.draftedFields,
    missingRequired: drafted.missingRequired,
    confidence: confidenceFor(selected, candidates, drafted.missingRequired),
    requiresReview: true,
  };
};
