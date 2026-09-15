import assert from 'node:assert/strict';
import { planWorkflow } from '../dist/index.mjs';

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
    name: 'notion_create_page',
    description: 'Create a Notion page with title and content',
    inputSchema: {
      type: 'object',
      properties: {
        title: { type: 'string' },
        content: { type: 'string' },
      },
      required: ['title', 'content'],
    },
    schema: schema({
      type: 'object',
      properties: {
        title: { type: 'string' },
        content: { type: 'string' },
      },
      required: ['title', 'content'],
    }),
  },
  {
    name: 'gmail_send_email',
    description: 'Send an email message to a recipient',
    inputSchema: {
      type: 'object',
      properties: {
        to: { type: 'string' },
        subject: { type: 'string' },
        body: { type: 'string' },
      },
      required: ['to', 'subject', 'body'],
    },
    schema: schema({
      type: 'object',
      properties: {
        to: { type: 'string' },
        subject: { type: 'string' },
        body: { type: 'string' },
      },
      required: ['to', 'subject', 'body'],
    }),
  },
];

const chained = planWorkflow(
  tools,
  'search GitHub issues for "bug" then send an email to paula@example.com with subject "Bug report" and body "Please review"',
);
assert.equal(chained.steps.length, 2);
assert.equal(chained.steps[0].kind, 'action');
assert.equal(chained.steps[0].action?.selected?.tool.name, 'github_search_issues');
assert.equal(chained.steps[1].kind, 'action');
assert.equal(chained.steps[1].action?.selected?.tool.name, 'gmail_send_email');
assert.equal(chained.steps[1].action?.arguments.to, 'paula@example.com');
assert.deepEqual(chained.steps[1].dependsOn, ['step-1']);
assert.equal(chained.requiresReview, true);
assert.equal(chained.autoExecutable, false);

const chinese = planWorkflow(tools, '查找 GitHub 的 bug issue，总结后写入 Notion');
assert.equal(chinese.steps.length, 3);
assert.equal(chinese.steps[0].kind, 'action');
assert.equal(chinese.steps[0].action?.selected?.tool.name, 'github_search_issues');
assert.equal(chinese.steps[1].kind, 'transform');
assert.equal(chinese.steps[1].transform?.operation, 'summarize');
assert.equal(chinese.steps[1].needsPreviousOutput, true);
assert.equal(chinese.steps[2].kind, 'action');
assert.equal(chinese.steps[2].action?.selected?.tool.name, 'notion_create_page');
assert.equal(chinese.steps[2].needsPreviousOutput, true);
assert.ok(chinese.steps[2].action?.missingRequired.includes('content'));
assert.ok(chinese.reviewReasons.some(reason => reason.includes('Cross-step output bindings')));
assert.ok(chinese.reviewReasons.some(reason => reason.includes('Local transform steps')));

const unresolved = planWorkflow(tools, 'quantum avocado resonance then summarize it');
assert.equal(unresolved.steps.length, 2);
assert.equal(unresolved.steps[0].kind, 'unresolved');
assert.equal(unresolved.steps[1].kind, 'transform');
assert.equal(unresolved.unresolvedStepCount, 1);
assert.equal(unresolved.confidence, 'low');

console.log('Workflow Planner smoke tests passed.');
