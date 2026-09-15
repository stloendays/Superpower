import assert from 'node:assert/strict';
import { planAction } from '../dist/index.mjs';

const tools = [
  {
    name: 'github_search_repositories',
    description: 'Search GitHub repositories by query',
    inputSchema: {
      type: 'object',
      properties: { query: { type: 'string' } },
      required: ['query'],
    },
    schema: JSON.stringify({
      type: 'object',
      properties: { query: { type: 'string' } },
      required: ['query'],
    }),
  },
  {
    name: 'github_get_issue',
    description: 'Read a GitHub issue from a repository',
    inputSchema: {
      type: 'object',
      properties: {
        owner: { type: 'string' },
        repo: { type: 'string' },
        issue_number: { type: 'integer' },
      },
      required: ['owner', 'repo', 'issue_number'],
    },
    schema: JSON.stringify({
      type: 'object',
      properties: {
        owner: { type: 'string' },
        repo: { type: 'string' },
        issue_number: { type: 'integer' },
      },
      required: ['owner', 'repo', 'issue_number'],
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
    schema: JSON.stringify({
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

const issuePlan = planAction(tools, '查看 GitHub 仓库 stloendays/Superpower-V1 的 issue #29');
assert.equal(issuePlan.selected?.tool.name, 'github_get_issue');
assert.equal(issuePlan.arguments.owner, 'stloendays');
assert.equal(issuePlan.arguments.repo, 'Superpower-V1');
assert.equal(issuePlan.arguments.issue_number, 29);
assert.deepEqual(issuePlan.missingRequired, []);
assert.equal(issuePlan.requiresReview, true);

const searchPlan = planAction(tools, 'search repositories for MCP browser tools');
assert.equal(searchPlan.selected?.tool.name, 'github_search_repositories');
assert.equal(typeof searchPlan.arguments.query, 'string');
assert.equal(searchPlan.missingRequired.length, 0);

const unmatched = planAction(tools, 'quantum avocado resonance');
assert.equal(unmatched.selected, null);
assert.equal(unmatched.confidence, 'low');
assert.equal(unmatched.requiresReview, true);

console.log('Action Planner smoke tests passed.');
