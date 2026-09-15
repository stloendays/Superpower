import type { ActionPlannerTool } from './action-planner.js';
import type { WorkflowBinding, WorkflowPlan, WorkflowPlanStep } from './workflow-planner.js';

export type WorkflowRunStatus = 'ready' | 'completed' | 'failed';
export type WorkflowRunStepStatus = 'pending' | 'completed' | 'failed';
export type WorkflowRunGateType =
  | 'ready'
  | 'binding_review_required'
  | 'binding_source_unavailable'
  | 'missing_required'
  | 'manual_input_required'
  | 'unresolved_step'
  | 'dependency_pending'
  | 'completed'
  | 'failed';

export interface WorkflowRunStepState {
  stepId: string;
  status: WorkflowRunStepStatus;
  output?: unknown;
  error?: string;
}

export interface WorkflowRunState<T extends ActionPlannerTool> {
  runId: string;
  plan: WorkflowPlan<T>;
  approvedBindingIds: string[];
  stepStates: WorkflowRunStepState[];
  status: WorkflowRunStatus;
  currentStepId: string | null;
  updatedAt: number;
}

export interface PreparedWorkflowRunStep<T extends ActionPlannerTool> {
  gate: WorkflowRunGateType;
  step: WorkflowPlanStep<T> | null;
  arguments: Record<string, unknown>;
  missingRequired: string[];
  requiredBindingIds: string[];
  unresolvedBindingIds: string[];
  reason: string;
}

const currentStepState = <T extends ActionPlannerTool>(state: WorkflowRunState<T>): WorkflowRunStepState | null =>
  state.currentStepId ? (state.stepStates.find(item => item.stepId === state.currentStepId) ?? null) : null;

const stepById = <T extends ActionPlannerTool>(
  state: WorkflowRunState<T>,
  stepId: string,
): WorkflowPlanStep<T> | null => state.plan.steps.find(step => step.id === stepId) ?? null;

const refreshedState = <T extends ActionPlannerTool>(state: WorkflowRunState<T>): WorkflowRunState<T> => {
  const failed = state.stepStates.find(step => step.status === 'failed');
  if (failed) {
    return {
      ...state,
      status: 'failed',
      currentStepId: failed.stepId,
      updatedAt: Date.now(),
    };
  }

  const next = state.stepStates.find(step => step.status !== 'completed');
  if (!next) {
    return {
      ...state,
      status: 'completed',
      currentStepId: null,
      updatedAt: Date.now(),
    };
  }

  return {
    ...state,
    status: 'ready',
    currentStepId: next.stepId,
    updatedAt: Date.now(),
  };
};

const resolvePath = (value: unknown, path: string): { found: boolean; value: unknown } => {
  if (path === '$') return { found: value !== undefined, value };
  if (!path.startsWith('$.')) return { found: false, value: undefined };

  const tokens = path
    .slice(2)
    .replace(/\[(\d+)\]/g, '.$1')
    .split('.')
    .filter(Boolean);
  let current: unknown = value;

  for (const token of tokens) {
    if (Array.isArray(current)) {
      const index = Number(token);
      if (!Number.isInteger(index) || index < 0 || index >= current.length) {
        return { found: false, value: undefined };
      }
      current = current[index];
      continue;
    }

    if (!current || typeof current !== 'object' || !(token in current)) {
      return { found: false, value: undefined };
    }
    current = (current as Record<string, unknown>)[token];
  }

  return { found: current !== undefined, value: current };
};

const asText = (value: unknown): string => {
  if (typeof value === 'string') return value;
  if (typeof value === 'number' || typeof value === 'boolean' || typeof value === 'bigint') return String(value);

  if (value && typeof value === 'object' && !Array.isArray(value)) {
    const content = (value as Record<string, unknown>).content;
    if (Array.isArray(content)) {
      const text = content
        .map(part => {
          if (!part || typeof part !== 'object' || Array.isArray(part)) return '';
          const candidate = (part as Record<string, unknown>).text;
          return typeof candidate === 'string' ? candidate : '';
        })
        .filter(Boolean)
        .join('\n');
      if (text) return text;
    }
  }

  try {
    return JSON.stringify(value);
  } catch {
    return String(value);
  }
};

const resolveBinding = <T extends ActionPlannerTool>(
  state: WorkflowRunState<T>,
  binding: WorkflowBinding,
): { found: boolean; value: unknown } => {
  const sourceState = state.stepStates.find(step => step.stepId === binding.sourceStepId);
  if (!sourceState || sourceState.status !== 'completed') return { found: false, value: undefined };
  const resolved = resolvePath(sourceState.output, binding.sourcePath);
  if (!resolved.found) return resolved;
  return {
    found: true,
    value: binding.coercion === 'text' ? asText(resolved.value) : resolved.value,
  };
};

const hasArgument = (args: Record<string, unknown>, name: string): boolean => {
  if (!(name in args)) return false;
  const value = args[name];
  return value !== undefined && value !== null && value !== '';
};

/**
 * Creates a session-only reviewed workflow run. Binding approvals are immutable for
 * this run so an output can never start flowing into a later argument without an
 * explicit approval made before execution begins.
 */
export const createWorkflowRunState = <T extends ActionPlannerTool>(
  plan: WorkflowPlan<T>,
  runId: string,
  approvedBindingIds: string[] = [],
): WorkflowRunState<T> => {
  const validBindingIds = new Set(plan.bindings.map(binding => binding.id));
  const approved = [...new Set(approvedBindingIds.filter(id => validBindingIds.has(id)))];

  return refreshedState({
    runId,
    plan,
    approvedBindingIds: approved,
    stepStates: plan.steps.map(step => ({ stepId: step.id, status: 'pending' as const })),
    status: 'ready',
    currentStepId: plan.steps[0]?.id ?? null,
    updatedAt: Date.now(),
  });
};

/**
 * Resolves the next step without executing it. A ready result contains the exact
 * arguments that the host may pass through the normal guarded MCP call path.
 */
export const prepareWorkflowRunStep = <T extends ActionPlannerTool>(
  state: WorkflowRunState<T>,
): PreparedWorkflowRunStep<T> => {
  if (state.status === 'completed') {
    return {
      gate: 'completed',
      step: null,
      arguments: {},
      missingRequired: [],
      requiredBindingIds: [],
      unresolvedBindingIds: [],
      reason: 'Workflow run is complete.',
    };
  }

  if (state.status === 'failed') {
    return {
      gate: 'failed',
      step: state.currentStepId ? stepById(state, state.currentStepId) : null,
      arguments: {},
      missingRequired: [],
      requiredBindingIds: [],
      unresolvedBindingIds: [],
      reason: currentStepState(state)?.error ?? 'Workflow run failed.',
    };
  }

  const step = state.currentStepId ? stepById(state, state.currentStepId) : null;
  if (!step) {
    return {
      gate: 'completed',
      step: null,
      arguments: {},
      missingRequired: [],
      requiredBindingIds: [],
      unresolvedBindingIds: [],
      reason: 'Workflow run is complete.',
    };
  }

  const pendingDependencies = step.dependsOn.filter(dependencyId => {
    const dependency = state.stepStates.find(item => item.stepId === dependencyId);
    return !dependency || dependency.status !== 'completed';
  });
  if (pendingDependencies.length > 0) {
    return {
      gate: 'dependency_pending',
      step,
      arguments: {},
      missingRequired: [],
      requiredBindingIds: [],
      unresolvedBindingIds: [],
      reason: `Waiting for dependencies: ${pendingDependencies.join(', ')}.`,
    };
  }

  const bindings = state.plan.bindings.filter(binding => binding.targetStepId === step.id);
  const requiredBindingIds = bindings.map(binding => binding.id);
  const approved = new Set(state.approvedBindingIds);
  const unresolvedBindingIds = bindings.filter(binding => !approved.has(binding.id)).map(binding => binding.id);
  if (unresolvedBindingIds.length > 0) {
    return {
      gate: 'binding_review_required',
      step,
      arguments: {},
      missingRequired: [],
      requiredBindingIds,
      unresolvedBindingIds,
      reason: 'One or more output bindings for this step were not explicitly approved for this run.',
    };
  }

  const resolvedBindings = new Map<string, unknown>();
  for (const binding of bindings) {
    const resolved = resolveBinding(state, binding);
    if (!resolved.found) {
      return {
        gate: 'binding_source_unavailable',
        step,
        arguments: {},
        missingRequired: [],
        requiredBindingIds,
        unresolvedBindingIds: [binding.id],
        reason: `Binding source ${binding.sourceStepId}${binding.sourcePath} is not available.`,
      };
    }
    resolvedBindings.set(binding.targetArgument, resolved.value);
  }

  if (step.kind === 'transform') {
    return {
      gate: 'manual_input_required',
      step,
      arguments: Object.fromEntries(resolvedBindings),
      missingRequired: [],
      requiredBindingIds,
      unresolvedBindingIds: [],
      reason: 'This transform requires explicit user-provided output in the current runner slice.',
    };
  }

  if (step.kind === 'unresolved' || !step.action?.selected) {
    return {
      gate: 'unresolved_step',
      step,
      arguments: {},
      missingRequired: [],
      requiredBindingIds,
      unresolvedBindingIds: [],
      reason: 'This step is not mapped to an MCP action and requires manual resolution.',
    };
  }

  const args: Record<string, unknown> = { ...step.action.arguments };
  resolvedBindings.forEach((value, targetArgument) => {
    if (targetArgument !== '$input') args[targetArgument] = value;
  });
  const missingRequired = step.action.missingRequired.filter(name => !hasArgument(args, name));
  if (missingRequired.length > 0) {
    return {
      gate: 'missing_required',
      step,
      arguments: args,
      missingRequired,
      requiredBindingIds,
      unresolvedBindingIds: [],
      reason: `Required arguments still need review: ${missingRequired.join(', ')}.`,
    };
  }

  return {
    gate: 'ready',
    step,
    arguments: args,
    missingRequired: [],
    requiredBindingIds,
    unresolvedBindingIds: [],
    reason: 'The next action is ready for one-step guarded execution.',
  };
};

export const recordWorkflowActionSuccess = <T extends ActionPlannerTool>(
  state: WorkflowRunState<T>,
  stepId: string,
  output: unknown,
): WorkflowRunState<T> => {
  if (state.currentStepId !== stepId) return state;
  const step = stepById(state, stepId);
  if (!step || step.kind !== 'action') return state;

  return refreshedState({
    ...state,
    stepStates: state.stepStates.map(item =>
      item.stepId === stepId ? { stepId, status: 'completed' as const, output } : item,
    ),
  });
};

export const recordWorkflowActionFailure = <T extends ActionPlannerTool>(
  state: WorkflowRunState<T>,
  stepId: string,
  error: string,
): WorkflowRunState<T> => {
  if (state.currentStepId !== stepId) return state;
  return refreshedState({
    ...state,
    stepStates: state.stepStates.map(item =>
      item.stepId === stepId ? { ...item, status: 'failed' as const, error } : item,
    ),
  });
};

/**
 * Marks a transform or unresolved step complete with output supplied explicitly by
 * the user/client. Action steps cannot be bypassed this way.
 */
export const provideWorkflowStepOutput = <T extends ActionPlannerTool>(
  state: WorkflowRunState<T>,
  stepId: string,
  output: unknown,
): WorkflowRunState<T> => {
  if (state.currentStepId !== stepId) return state;
  const step = stepById(state, stepId);
  if (!step || step.kind === 'action') return state;

  return refreshedState({
    ...state,
    stepStates: state.stepStates.map(item =>
      item.stepId === stepId ? { stepId, status: 'completed' as const, output } : item,
    ),
  });
};
