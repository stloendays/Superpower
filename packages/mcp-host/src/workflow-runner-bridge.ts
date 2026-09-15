import {
  createWorkflowRunState,
  prepareWorkflowRunStep,
  provideWorkflowStepOutput,
  recordWorkflowActionFailure,
  recordWorkflowActionSuccess,
  type PreparedWorkflowRunStep,
  type SdkRoutableTool,
  type WorkflowPlan,
  type WorkflowRunState,
} from '@superpower/mcp-core';
import type { ConnectedSuperpowerHost } from './host.js';

type WorkflowToolCaller = (toolName: string, args: Record<string, unknown>, approve: boolean) => Promise<unknown>;

type StoredRun = {
  planId: string;
  state: WorkflowRunState<SdkRoutableTool>;
};

const MAX_STORED_PLANS = 20;
const MAX_STORED_RUNS = 20;

const policyWire = (policy: ReturnType<ConnectedSuperpowerHost['gateway']['evaluate']>) => ({
  decision: policy.decision,
  risk: policy.risk,
  reasons: policy.reasons,
  sensitiveArgumentKeys: policy.sensitiveArgumentKeys,
});

const trimOldest = <T>(map: Map<string, T>, maxSize: number): void => {
  while (map.size > maxSize) {
    const oldest = map.keys().next().value as string | undefined;
    if (!oldest) return;
    map.delete(oldest);
  }
};

export class WorkflowRunRegistry {
  private readonly plans = new Map<string, WorkflowPlan<SdkRoutableTool>>();
  private readonly runs = new Map<string, StoredRun>();
  private nextPlanId = 1;
  private nextRunId = 1;

  constructor(
    private readonly host: ConnectedSuperpowerHost,
    private readonly callTool: WorkflowToolCaller,
  ) {}

  hasPlan(planId: string): boolean {
    return this.plans.has(planId);
  }

  hasRun(runId: string): boolean {
    return this.runs.has(runId);
  }

  async plan(query: string): Promise<Record<string, unknown>> {
    const workflow = await this.host.gateway.planWorkflow(query, { maxCandidates: 5, maxSteps: 6 });
    const planId = `workflow-plan-${Date.now()}-${this.nextPlanId++}`;
    this.plans.set(planId, workflow);
    trimOldest(this.plans, MAX_STORED_PLANS);

    return this.serializePlan(planId, workflow);
  }

  start(planId: string, approvedBindingIds: string[]): Record<string, unknown> {
    const plan = this.plans.get(planId);
    if (!plan) throw new Error('Workflow plan is not available in this MCP session. Re-plan the workflow.');

    const validBindingIds = new Set(plan.bindings.map(binding => binding.id));
    const unknownBindingIds = approvedBindingIds.filter(id => !validBindingIds.has(id));
    if (unknownBindingIds.length > 0) {
      throw new Error(`Unknown workflow binding ids: ${unknownBindingIds.join(', ')}.`);
    }

    const runId = `workflow-run-${Date.now()}-${this.nextRunId++}`;
    const state = createWorkflowRunState(plan, runId, approvedBindingIds);
    this.runs.set(runId, { planId, state });
    trimOldest(this.runs, MAX_STORED_RUNS);
    return this.serializeRun(this.runs.get(runId)!);
  }

  status(runId: string): Record<string, unknown> {
    return this.serializeRun(this.requireRun(runId));
  }

  async advance(runId: string, approve: boolean): Promise<Record<string, unknown>> {
    const stored = this.requireRun(runId);
    const prepared = prepareWorkflowRunStep(stored.state);
    const step = prepared.step;
    const action = step?.action;
    const selected = action?.selected;
    if (prepared.gate !== 'ready' || !step || !action || !selected) {
      return this.serializeRun(stored, prepared);
    }

    const policy = this.host.gateway.evaluate(selected.tool.name, prepared.arguments, selected.tool.description ?? '');
    if (policy.decision === 'confirm' && !approve) {
      return this.serializeRun(stored, prepared, {
        type: 'confirmation_required',
        reason: 'The guarded execution policy requires explicit confirmation for this step.',
        policy: policyWire(policy),
      });
    }

    try {
      const output = await this.callTool(selected.tool.name, prepared.arguments, approve);
      stored.state = recordWorkflowActionSuccess(stored.state, step.id, output);
      return this.serializeRun(stored, undefined, {
        type: 'step_completed',
        stepId: step.id,
        toolName: selected.tool.name,
        output,
      });
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      stored.state = recordWorkflowActionFailure(stored.state, step.id, message);
      return this.serializeRun(stored, undefined, {
        type: 'step_failed',
        stepId: step.id,
        toolName: selected.tool.name,
        error: message,
      });
    }
  }

  provideOutput(runId: string, stepId: string, output: unknown): Record<string, unknown> {
    const stored = this.requireRun(runId);
    const prepared = prepareWorkflowRunStep(stored.state);
    if (!prepared.step || prepared.step.id !== stepId) {
      throw new Error('Manual workflow output must target the current step.');
    }
    if (prepared.gate !== 'manual_input_required' && prepared.gate !== 'unresolved_step') {
      throw new Error('The current workflow step does not accept manual output.');
    }

    stored.state = provideWorkflowStepOutput(stored.state, stepId, output);
    return this.serializeRun(stored, undefined, {
      type: 'manual_output_recorded',
      stepId,
    });
  }

  private requireRun(runId: string): StoredRun {
    const stored = this.runs.get(runId);
    if (!stored) throw new Error('Workflow run is not available in this MCP session. Start a new reviewed run.');
    return stored;
  }

  private serializePlan(planId: string, workflow: WorkflowPlan<SdkRoutableTool>): Record<string, unknown> {
    return {
      planId,
      transport: this.host.transportKind,
      query: workflow.query,
      confidence: workflow.confidence,
      requiresReview: workflow.requiresReview,
      autoExecutable: workflow.autoExecutable,
      unresolvedStepCount: workflow.unresolvedStepCount,
      reviewReasons: workflow.reviewReasons,
      bindings: workflow.bindings,
      steps: workflow.steps.map(step => {
        const action = step.action;
        const selected = action?.selected ?? null;
        const policy = selected
          ? this.host.gateway.evaluate(selected.tool.name, action?.arguments ?? {}, selected.tool.description ?? '')
          : null;

        return {
          id: step.id,
          index: step.index,
          instruction: step.instruction,
          kind: step.kind,
          dependsOn: step.dependsOn,
          needsPreviousOutput: step.needsPreviousOutput,
          confidence: step.confidence,
          requiresReview: step.requiresReview,
          transform: step.transform,
          action: action
            ? {
                query: action.query,
                selected: selected
                  ? {
                      name: selected.tool.name,
                      description: selected.tool.description ?? '',
                      inputSchema: selected.tool.inputSchema ?? {},
                      score: selected.score,
                      matchedTerms: selected.matchedTerms,
                    }
                  : null,
                candidates: action.candidates.map(item => ({
                  name: item.tool.name,
                  description: item.tool.description ?? '',
                  score: item.score,
                  matchedTerms: item.matchedTerms,
                })),
                arguments: action.arguments,
                draftedFields: action.draftedFields,
                missingRequired: action.missingRequired,
                confidence: action.confidence,
                requiresReview: action.requiresReview,
                policy: policy ? policyWire(policy) : null,
              }
            : null,
        };
      }),
    };
  }

  private serializeRun(
    stored: StoredRun,
    prepared: PreparedWorkflowRunStep<SdkRoutableTool> = prepareWorkflowRunStep(stored.state),
    event?: Record<string, unknown>,
  ): Record<string, unknown> {
    const currentStep = prepared.step;
    const currentPolicy =
      prepared.gate === 'ready' && currentStep?.action?.selected
        ? this.host.gateway.evaluate(
            currentStep.action.selected.tool.name,
            prepared.arguments,
            currentStep.action.selected.tool.description ?? '',
          )
        : null;

    return {
      runId: stored.state.runId,
      planId: stored.planId,
      status: stored.state.status,
      currentStepId: stored.state.currentStepId,
      approvedBindingIds: stored.state.approvedBindingIds,
      updatedAt: stored.state.updatedAt,
      steps: stored.state.stepStates.map(step => ({
        stepId: step.stepId,
        status: step.status,
        ...(step.error ? { error: step.error } : {}),
      })),
      gate: {
        type: prepared.gate,
        reason: prepared.reason,
        stepId: currentStep?.id ?? null,
        kind: currentStep?.kind ?? null,
        instruction: currentStep?.instruction ?? '',
        arguments: prepared.arguments,
        missingRequired: prepared.missingRequired,
        requiredBindingIds: prepared.requiredBindingIds,
        unresolvedBindingIds: prepared.unresolvedBindingIds,
        ...(currentPolicy ? { policy: policyWire(currentPolicy) } : {}),
      },
      ...(event ? { event } : {}),
    };
  }
}
