import assert from 'node:assert/strict';
import { evaluateToolExecution } from '../dist/index.mjs';

const guarded = toolName => evaluateToolExecution(toolName, {}, '', 'guarded');

const snakeCaseDelete = guarded('delete_account');
assert.equal(snakeCaseDelete.risk, 'critical');
assert.equal(snakeCaseDelete.decision, 'confirm');

const camelCaseDelete = guarded('deleteAccount');
assert.equal(camelCaseDelete.risk, 'critical');
assert.equal(camelCaseDelete.decision, 'confirm');

const destructiveDescription = evaluateToolExecution(
  'manageAccount',
  {},
  'Delete an account permanently after validation',
  'guarded',
);
assert.equal(destructiveDescription.risk, 'critical');
assert.equal(destructiveDescription.decision, 'confirm');

const sendEmail = guarded('sendEmail');
assert.equal(sendEmail.risk, 'high');
assert.equal(sendEmail.decision, 'confirm');

const benignRead = guarded('read_file');
assert.equal(benignRead.risk, 'low');
assert.equal(benignRead.decision, 'allow');

const dropboxSearch = guarded('dropbox_search');
assert.equal(dropboxSearch.risk, 'low');
assert.equal(dropboxSearch.decision, 'allow');

const sensitiveRead = evaluateToolExecution('read_profile', { auth: { apiKey: 'not-inspected' } }, '', 'guarded');
assert.equal(sensitiveRead.risk, 'medium');
assert.equal(sensitiveRead.decision, 'allow');
assert.deepEqual(sensitiveRead.sensitiveArgumentKeys, ['auth.apiKey']);

const auditDelete = evaluateToolExecution('deleteAccount', {}, '', 'audit');
assert.equal(auditDelete.risk, 'critical');
assert.equal(auditDelete.decision, 'allow');

console.log('Execution Policy smoke tests passed.');
