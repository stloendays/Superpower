import assert from 'node:assert/strict';
import {
  createWorkflowRunState,
  planWorkflow,
  prepareWorkflowRunStep,
  provideWorkflowStepOutput,
  recordWorkflowActionSuccess,
} from '../dist/index.mjs';

const schema = value => JSON.stringify(value);

const tools = [
  {
    name: 'github_search_issues',
    description: 'Search GitHub issues by query',
    inputSchema: {
      type: 'object',
      properties: { query: { type: 'string' } },
      required: ['query'],
    },
    schema: schema({
      type: 'object',
      properties: { query: { type: 'string' } },
      required: ['query'],
    }),
  },
  {
    name: 'notion_create_note',
    description: 'Write content into a Notion note',
    inputSchema: {
      type: 'object',
      properties: { content: { type: 'string' } },
      required: ['content'],
    },
    schema: schema({
      type: 'object',
      properties: { content: { type: 'string' } },
      required: ['content'],
    }),
  },
];

const plan = planWorkflow(tools, 'search GitHub issues for "bug" then write the results into Notion');
assert.equal(plan.steps.length, 2);
assert.equal(plan.bindings.length, 1);
assert.equal(plan.bindings[0].sourceStepId, 'step-1');
assert.equal(plan.bindings[0].targetStepId, 'step-2');
assert.equal(plan.bindings[0].targetArgument, 'content');
assert.equal(plan.bindings[0].coercion, 'text');

let run = createWorkflowRunState(plan, 'run-1', [plan.bindings[0].id]);
let prepared = prepareWorkflowRunStep(run);
assert.equal(prepared.gate, 'ready');
assert.equal(prepared.step?.id, 'step-1');

run = recordWorkflowActionSuccess(run, 'step-1', {
  content: [
    { type: 'text', text: 'Issue A' },
    { type: 'text', text: 'Issue B' },
  ],
});
prepared = prepareWorkflowRunStep(run);
assert.equal(prepared.gate, 'ready');
assert.equal(prepared.step?.id, 'step-2');
assert.equal(prepared.arguments.content, 'Issue A\nIssue B');

run = recordWorkflowActionSuccess(run, 'step-2', { ok: true });
assert.equal(run.status, 'completed');
assert.equal(run.currentStepId, null);
assert.equal(prepareWorkflowRunStep(run).gate, 'completed');

let unapproved = createWorkflowRunState(plan, 'run-2');
unapproved = recordWorkflowActionSuccess(unapproved, 'step-1', { content: [{ type: 'text', text: 'Issue A' }] });
const blocked = prepareWorkflowRunStep(unapproved);
assert.equal(blocked.gate, 'binding_review_required');
assert.deepEqual(blocked.unresolvedBindingIds, [plan.bindings[0].id]);

const transformPlan = planWorkflow(tools, 'search GitHub issues for "bug" then summarize it');
assert.equal(transformPlan.steps[1].kind, 'transform');
assert.equal(transformPlan.bindings.length, 1);
let transformRun = createWorkflowRunState(transformPlan, 'run-3', [transformPlan.bindings[0].id]);
transformRun = recordWorkflowActionSuccess(transformRun, 'step-1', {
  content: [{ type: 'text', text: 'A long issue description' }],
});
const transformPrepared = prepareWorkflowRunStep(transformRun);
assert.equal(transformPrepared.gate, 'manual_input_required');
assert.equal(transformPrepared.arguments.$input, 'A long issue description');
transformRun = provideWorkflowStepOutput(transformRun, 'step-2', 'Short summary');
assert.equal(transformRun.status, 'completed');

console.log('Workflow Runner smoke tests passed.');
