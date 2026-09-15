import {
  planAction,
  type ActionPlan,
  type ActionPlanConfidence,
  type ActionPlannerOptions,
  type ActionPlannerTool,
} from './action-planner.js';

export type WorkflowStepKind = 'action' | 'transform' | 'unresolved';
export type WorkflowTransformOperation = 'summarize' | 'rewrite' | 'extract' | 'format';
export type WorkflowBindingCoercion = 'raw' | 'text';

export interface WorkflowBinding {
  id: string;
  sourceStepId: string;
  sourcePath: string;
  targetStepId: string;
  targetArgument: string;
  coercion: WorkflowBindingCoercion;
  requiresReview: true;
}

export interface WorkflowPlannerOptions extends ActionPlannerOptions {
  maxSteps?: number;
  /** Minimum routed score required before a clause becomes an MCP action step. */
  actionMinScore?: number;
}

export interface WorkflowPlanStep<T extends ActionPlannerTool> {
  id: string;
  index: number;
  instruction: string;
  kind: WorkflowStepKind;
  dependsOn: string[];
  needsPreviousOutput: boolean;
  confidence: ActionPlanConfidence;
  requiresReview: true;
  action: ActionPlan<T> | null;
  transform: {
    operation: WorkflowTransformOperation;
    instruction: string;
  } | null;
}

export interface WorkflowPlan<T extends ActionPlannerTool> {
  query: string;
  steps: WorkflowPlanStep<T>[];
  bindings: WorkflowBinding[];
  confidence: ActionPlanConfidence;
  requiresReview: true;
  autoExecutable: false;
  unresolvedStepCount: number;
  reviewReasons: string[];
}

const canonicalKey = (value: string): string => value.toLowerCase().replace(/[^a-z0-9]/g, '');

const splitWorkflowClauses = (query: string, maxSteps: number): string[] => {
  const expanded = query
    .replace(
      /(?:，|,)?\s*(总结|概括|摘要)(?:后|之后|并(?:且)?|再)?(?=\s*(?:写|保存|创建|发送|发|write|save|create|send))/gi,
      '|||$1|||',
    )
    .replace(
      /\b(summarize|summarise)\b(?:\s+(?:it|them|the\s+results?))?\s+(?:and\s+then|then|and)\s+(?=(?:write|save|create|send)\b)/gi,
      '|||$1|||',
    );

  const clauses = expanded
    .split(
      /\s*(?:\|\|\||->|→|;|\n+|\b(?:and\s+then|then|after\s+that|next|finally)\b|然后|接着|随后|最后|再把|再将|并把|并将)\s*/i,
    )
    .map(value => value.replace(/^[，,。.!?？]+|[，,。.!?？]+$/g, '').trim())
    .filter(Boolean);

  return clauses.slice(0, Math.max(1, maxSteps));
};

const detectTransform = (instruction: string): WorkflowTransformOperation | null => {
  if (/\b(?:summarize|summarise|summary)\b|总结|概括|摘要/i.test(instruction)) return 'summarize';
  if (/\b(?:rewrite|rephrase)\b|改写|重写/i.test(instruction)) return 'rewrite';
  if (/\b(?:extract|pull\s+out)\b|提取|抽取/i.test(instruction)) return 'extract';
  if (/\b(?:format|convert)\b|格式化|整理成|转换成/i.test(instruction)) return 'format';
  return null;
};

const refersToPreviousOutput = (instruction: string): boolean =>
  /\b(?:it|them|those|these|result|results|output|previous|above)\b|它们?|这些|那些|结果|输出|上一步|前一步|刚才/i.test(
    instruction,
  );

const missingLooksLikeHandoff = (fields: string[]): boolean =>
  fields.some(field => {
    const key = canonicalKey(field);
    return (
      key.includes('content') ||
      key.includes('body') ||
      key.includes('text') ||
      key.includes('message') ||
      key.includes('summary') ||
      key.includes('description') ||
      key.includes('input') ||
      key.includes('data')
    );
  });

const handoffFields = (fields: string[]): string[] =>
  fields.filter(field => {
    const key = canonicalKey(field);
    return (
      key.includes('content') ||
      key.includes('body') ||
      key.includes('text') ||
      key.includes('message') ||
      key.includes('summary') ||
      key.includes('description') ||
      key.includes('input') ||
      key.includes('data')
    );
  });

const workflowConfidence = <T extends ActionPlannerTool>(steps: WorkflowPlanStep<T>[]): ActionPlanConfidence => {
  if (steps.length === 0 || steps.some(step => step.kind === 'unresolved' || step.confidence === 'low')) return 'low';
  if (steps.some(step => step.kind === 'transform' || step.confidence === 'medium' || step.needsPreviousOutput)) {
    return 'medium';
  }
  return 'high';
};

const buildBindings = <T extends ActionPlannerTool>(steps: WorkflowPlanStep<T>[]): WorkflowBinding[] => {
  const bindings: WorkflowBinding[] = [];

  steps.forEach((step, index) => {
    if (index === 0 || !step.needsPreviousOutput) return;
    const sourceStepId = step.dependsOn[step.dependsOn.length - 1] ?? steps[index - 1]?.id;
    if (!sourceStepId) return;

    if (step.kind === 'transform') {
      bindings.push({
        id: `${sourceStepId}-to-${step.id}-input`,
        sourceStepId,
        sourcePath: '$',
        targetStepId: step.id,
        targetArgument: '$input',
        coercion: 'text',
        requiresReview: true,
      });
      return;
    }

    if (step.kind !== 'action' || !step.action) return;
    handoffFields(step.action.missingRequired).forEach(field => {
      bindings.push({
        id: `${sourceStepId}-to-${step.id}-${canonicalKey(field) || 'value'}`,
        sourceStepId,
        sourcePath: '$',
        targetStepId: step.id,
        targetArgument: field,
        coercion: 'text',
        requiresReview: true,
      });
    });
  });

  return bindings;
};

/**
 * Deterministic zero-model workflow planner.
 *
 * It decomposes explicit multi-step language, reuses the existing Action Planner
 * for every MCP action clause, and represents local text transformations without
 * pretending that they are executable MCP tools. Cross-step output handoffs are
 * represented as explicit review-required bindings instead of hidden data plumbing.
 * The returned plan is never auto-executable; native clients must keep using the
 * normal guarded call path.
 */
export const planWorkflow = <T extends ActionPlannerTool>(
  tools: T[],
  query: string,
  options: WorkflowPlannerOptions = {},
): WorkflowPlan<T> => {
  const trimmed = query.trim();
  const maxSteps = Math.min(8, Math.max(1, options.maxSteps ?? 6));
  const actionMinScore = Math.max(1, options.actionMinScore ?? 10);
  const clauses = splitWorkflowClauses(trimmed, maxSteps);

  const steps: WorkflowPlanStep<T>[] = clauses.map((instruction, index) => {
    const id = `step-${index + 1}`;
    const action = planAction(tools, instruction, {
      maxCandidates: options.maxCandidates ?? 5,
      minScore: options.minScore ?? 1,
    });
    const selectedScore = action.selected?.score ?? 0;
    const transformOperation = detectTransform(instruction);

    let kind: WorkflowStepKind = 'unresolved';
    let transform: WorkflowPlanStep<T>['transform'] = null;
    let confidence: ActionPlanConfidence = 'low';
    let actionPlan: ActionPlan<T> | null = action;

    if (transformOperation) {
      kind = 'transform';
      confidence = 'medium';
      actionPlan = null;
      transform = { operation: transformOperation, instruction };
    } else if (action.selected && selectedScore >= actionMinScore) {
      kind = 'action';
      confidence = action.confidence;
    }

    const needsPreviousOutput =
      index > 0 &&
      (kind === 'transform' ||
        refersToPreviousOutput(instruction) ||
        (kind === 'action' && missingLooksLikeHandoff(action.missingRequired)));

    return {
      id,
      index: index + 1,
      instruction,
      kind,
      dependsOn: index === 0 ? [] : [`step-${index}`],
      needsPreviousOutput,
      confidence,
      requiresReview: true,
      action: actionPlan,
      transform,
    };
  });

  const bindings = buildBindings(steps);
  const unresolvedStepCount = steps.filter(step => step.kind === 'unresolved').length;
  const reviewReasons = [
    'Workflow plans are review-first and never execute tools automatically.',
    'Every MCP action still uses the existing guarded execution policy.',
  ];
  if (steps.some(step => step.needsPreviousOutput)) {
    reviewReasons.push('Cross-step output bindings require explicit review before they can be executed.');
  }
  if (bindings.length > 0) {
    reviewReasons.push('Every proposed output binding is visible and must be explicitly approved for a run session.');
  }
  if (steps.some(step => step.kind === 'transform')) {
    reviewReasons.push('Local transform steps require explicit user-provided output in the first Workflow Runner slice.');
  }
  if (unresolvedStepCount > 0) {
    reviewReasons.push('One or more workflow steps could not be mapped confidently to an MCP action.');
  }

  return {
    query: trimmed,
    steps,
    bindings,
    confidence: workflowConfidence(steps),
    requiresReview: true,
    autoExecutable: false,
    unresolvedStepCount,
    reviewReasons,
  };
};
