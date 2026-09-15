import { jsonSchemaToCsn } from './schema_converter';
import { chatgptInstructions } from './website_specific_instruction/chatgpt';
import { geminiInstructions } from './website_specific_instruction/gemini';
import {
  getContextBudgetReport,
  resolveContextBudget,
  truncateFreeText,
  type ContextBudgetConfig,
} from '../../../core/context-budget';
import { routeTools } from '../../../core/tool-router';
import { createLogger } from '@extension/shared/lib/logger';

const logger = createLogger('InstructionGeneratorJSON');

export interface InstructionTool {
  name: string;
  schema: string;
  description: string;
}

export interface InstructionGenerationOptions {
  /** Optional task text used by the local Tool Router. Empty means preserve tool order. */
  routingQuery?: string;
  maxTools?: number;
  maxInstructionChars?: number;
}

export interface InstructionGenerationStats {
  totalTools: number;
  selectedTools: number;
  omittedByRouter: number;
  omittedByBudget: number;
  routingActive: boolean;
  customInstructionsTruncated: boolean;
  chars: number;
  estimatedTokens: number;
  budgetUtilization: number;
}

export interface InstructionGenerationResult {
  instructions: string;
  stats: InstructionGenerationStats;
}

const BASE_INSTRUCTIONS = `[SuperAssistant Operational Instructions][IMPORTANT]

You are SuperAssistant. Use the MCP tools listed below when they materially help answer the user's request.

Tool-call protocol:
- Emit tool calls only inside fenced \`\`\`jsonl blocks.
- Put exactly one complete tool call in each fenced block.
- A call starts with {"type":"function_call_start","name":"...","call_id":N}.
- Then emit one optional description event and one parameter event per supplied argument.
- End with {"type":"function_call_end","call_id":N}.
- Use monotonically increasing integer call_id values, starting at 1 for a fresh session.
- Include every required parameter. Omit optional parameters unless useful.
- Preserve explicitly supplied user values exactly. Objects and arrays must be valid JSON values.
- Never invent a tool, parameter, or function result.
- Emit at most five independent calls per response. Stop before any call that depends on an earlier result.
- After emitting tool calls, stop until <function_results> are returned.
- Do not print tool-call syntax in reasoning or examples unless actually requesting execution.
`;

const CSN_LEGEND =
  'Schema notation: o=object, s=string, i=integer, n=number, b=boolean, a[]=array, e[]=enum, r=required, ?=optional.';

const MAX_PARAMETER_NOTES = 10;
const MAX_PARAMETER_NOTES_CHARS = 480;
const MAX_PARAMETER_DESCRIPTION_CHARS = 120;

const normalizeText = (value: string, maxLength = 280): string => truncateFreeText(value || '', maxLength);

const compactType = (definition: any): string => {
  if (!definition || typeof definition !== 'object') return 'any';
  if (Array.isArray(definition.enum)) return `enum[${definition.enum.slice(0, 6).map(String).join('|')}]`;
  if (definition.type === 'array') return `array<${compactType(definition.items)}>`;
  if (definition.type === 'object') return 'object';
  return definition.type || 'any';
};

/** Safe fallback for unusually large or unsupported schemas. It keeps complete top-level parameter names/types. */
const summarizeSchema = (schema: any): string => {
  if (!schema || typeof schema !== 'object') return 'object';
  const properties = schema.properties && typeof schema.properties === 'object' ? schema.properties : {};
  const required = new Set(Array.isArray(schema.required) ? schema.required : []);
  const parts = Object.entries(properties).map(
    ([name, definition]) => `${name}:${compactType(definition)}${required.has(name) ? ' r' : ''}`,
  );
  return parts.length > 0 ? `o { ${parts.join('; ')} }` : compactType(schema);
};

const isUsableCompactSchema = (value: string): boolean => {
  const normalized = value.trim();
  if (!normalized || normalized === 'undefined') return false;

  // jsonSchemaToCsn intentionally supports a compact subset of JSON Schema.
  // Unsupported nested constructs can otherwise leak the literal word
  // "undefined" into an apparently valid schema string.
  return !/(^|[:\[,\s])undefined(?=$|[;\]},\]\s])/.test(normalized);
};

/**
 * Preserve a small amount of parameter semantics that structural schema
 * compression cannot encode. Notes are bounded globally per tool and recurse at
 * most one nested level, so useful descriptions do not defeat context savings.
 */
const getParameterNotes = (schema: any): string => {
  const notes: string[] = [];

  const visit = (node: any, prefix = '', depth = 0) => {
    if (!node || typeof node !== 'object' || notes.length >= MAX_PARAMETER_NOTES) return;
    const properties = node.properties && typeof node.properties === 'object' ? node.properties : {};

    for (const [name, definition] of Object.entries(properties) as Array<[string, any]>) {
      if (notes.length >= MAX_PARAMETER_NOTES) break;

      const path = prefix ? `${prefix}.${name}` : name;
      const description = normalizeText(definition?.description || '', MAX_PARAMETER_DESCRIPTION_CHARS);
      if (description) notes.push(`${path}: ${description}`);

      if (depth >= 1 || notes.length >= MAX_PARAMETER_NOTES) continue;

      if (definition?.type === 'object' && definition.properties) {
        visit(definition, path, depth + 1);
      } else if (definition?.type === 'array' && definition.items?.type === 'object') {
        visit(definition.items, `${path}[]`, depth + 1);
      }
    }
  };

  visit(schema);
  return notes.length > 0 ? truncateFreeText(notes.join('; '), MAX_PARAMETER_NOTES_CHARS) : '';
};

const buildToolEntry = (name: string, description: string, schema: string, parameterNotes = ''): string => {
  const lines = [`- ${name}${description ? ` — ${description}` : ''}`, `  schema: ${schema}`];
  if (parameterNotes) lines.push(`  params: ${parameterNotes}`);
  return lines.join('\n');
};

const formatTool = (tool: InstructionTool, budget: ContextBudgetConfig): string => {
  const name = normalizeText(tool.name, 120);
  const description = normalizeText(tool.description || '', 280);

  try {
    const parsedSchema = JSON.parse(tool.schema || '{}');
    const parameterNotes = getParameterNotes(parsedSchema);

    let compactSchema: string;
    try {
      const converted = jsonSchemaToCsn(parsedSchema);
      compactSchema = isUsableCompactSchema(converted) ? converted : summarizeSchema(parsedSchema);
      if (!isUsableCompactSchema(converted)) {
        logger.debug(`[InstructionGenerator] Falling back to schema summary for ${tool.name}`);
      }
    } catch (error) {
      logger.warn(`Unable to convert schema for ${tool.name}; using safe summary.`, error);
      compactSchema = summarizeSchema(parsedSchema);
    }

    const fullEntry = buildToolEntry(name, description, compactSchema, parameterNotes);
    if (fullEntry.length <= budget.maxToolChars) return fullEntry;

    const reducedDescription = normalizeText(tool.description || '', 140);
    const reducedNotes = parameterNotes ? truncateFreeText(parameterNotes, 240) : '';
    const reducedEntry = buildToolEntry(name, reducedDescription, summarizeSchema(parsedSchema), reducedNotes);
    if (reducedEntry.length <= budget.maxToolChars) return reducedEntry;

    // Semantics are useful, but never let them be the reason an otherwise usable
    // tool is excluded from the overall context budget.
    return buildToolEntry(name, reducedDescription, summarizeSchema(parsedSchema));
  } catch (error) {
    logger.warn(`Unable to parse schema for ${tool.name}:`, error);
    return buildToolEntry(name, description, 'unavailable');
  }
};

const getSiteInstructions = (): string[] => {
  const sections: string[] = [];
  const currentHost = typeof window !== 'undefined' ? window.location.hostname : '';
  if (currentHost.includes('gemini')) sections.push(geminiInstructions.trim());
  if (currentHost.includes('chatgpt')) sections.push(chatgptInstructions.trim());
  return sections;
};

export const generateInstructionsJsonDetailed = (
  tools: InstructionTool[],
  customInstructions?: string,
  customInstructionsEnabled?: boolean,
  options: InstructionGenerationOptions = {},
): InstructionGenerationResult => {
  const budget = resolveContextBudget({
    maxInstructionChars: options.maxInstructionChars,
  });

  if (!tools || tools.length === 0) {
    const instructions = '# No tools available\n\nConnect to the MCP server to see available tools.';
    const report = getContextBudgetReport(instructions, budget);
    return {
      instructions,
      stats: {
        totalTools: 0,
        selectedTools: 0,
        omittedByRouter: 0,
        omittedByBudget: 0,
        routingActive: false,
        customInstructionsTruncated: false,
        chars: report.chars,
        estimatedTokens: report.estimatedTokens,
        budgetUtilization: report.utilization,
      },
    };
  }

  const routingQuery = options.routingQuery?.trim() ?? '';
  const requestedMaxTools = options.maxTools ?? (routingQuery ? 12 : budget.maxToolCount);
  const route = routeTools(tools, routingQuery, {
    maxTools: Math.min(Math.max(1, requestedMaxTools), budget.maxToolCount),
  });

  const sections: string[] = [BASE_INSTRUCTIONS.trim(), ...getSiteInstructions()];
  let customInstructionsTruncated = false;
  if (customInstructionsEnabled && customInstructions?.trim()) {
    const trimmed = customInstructions.trim();
    const bounded = truncateFreeText(trimmed, budget.maxCustomInstructionChars);
    customInstructionsTruncated = bounded !== trimmed.replace(/\s+/g, ' ').trim();
    sections.push(`<custom_instructions>\n${bounded}\n</custom_instructions>`);
  }

  const toolHeader = `## AVAILABLE MCP TOOLS\n${CSN_LEGEND}`;
  const tail = 'User interaction starts here:';
  const fixedChars = [...sections, toolHeader, tail].join('\n\n').length + 2;
  const availableToolChars = Math.max(0, budget.maxInstructionChars - fixedChars - 120);

  const renderedTools: string[] = [];
  let usedToolChars = 0;
  let omittedByBudget = 0;

  route.tools.forEach(tool => {
    const entry = formatTool(tool, budget);
    const incremental = entry.length + (renderedTools.length > 0 ? 1 : 0);
    if (usedToolChars + incremental <= availableToolChars) {
      renderedTools.push(entry);
      usedToolChars += incremental;
    } else {
      omittedByBudget += 1;
    }
  });

  // Guarantee at least a discoverable tool name when a very small custom budget is supplied.
  if (renderedTools.length === 0 && route.tools.length > 0 && availableToolChars > 40) {
    const minimal = `- ${normalizeText(route.tools[0].name, Math.max(20, availableToolChars - 2))}`;
    renderedTools.push(minimal);
    omittedByBudget = Math.max(0, route.tools.length - 1);
  }

  const omittedTotal = route.omitted + omittedByBudget;
  const omissionNote =
    omittedTotal > 0
      ? `\n\n[Context optimization: ${omittedTotal} tool${omittedTotal === 1 ? '' : 's'} omitted. Set a task focus or enable fewer tools to change the selection.]`
      : '';

  const toolSection = `${toolHeader}\n\n${renderedTools.join('\n')}${omissionNote}`;
  const instructions = `${[...sections, toolSection, tail].filter(Boolean).join('\n\n')}\n`;
  const report = getContextBudgetReport(instructions, budget);

  return {
    instructions,
    stats: {
      totalTools: tools.length,
      selectedTools: renderedTools.length,
      omittedByRouter: route.omitted,
      omittedByBudget,
      routingActive: route.queryUsed,
      customInstructionsTruncated,
      chars: report.chars,
      estimatedTokens: report.estimatedTokens,
      budgetUtilization: report.utilization,
    },
  };
};

export const generateInstructionsJson = (
  tools: InstructionTool[],
  customInstructions?: string,
  customInstructionsEnabled?: boolean,
  options: InstructionGenerationOptions = {},
): string =>
  generateInstructionsJsonDetailed(tools, customInstructions, customInstructionsEnabled, options).instructions;
