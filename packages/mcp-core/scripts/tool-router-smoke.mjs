import assert from 'node:assert/strict';
import { routeTools } from '../dist/index.mjs';

const tools = [
  { name: 'list_files', description: 'List files in a folder' },
  { name: 'create_issue', description: 'Create a repository issue' },
  { name: 'update_record', description: 'Update a database record' },
  { name: 'calendar_event', description: 'Create or inspect a calendar event' },
  { name: 'search_docs', description: 'Search documentation' },
  { name: 'export_report', description: 'Export a report file' },
  { name: 'send_email', description: 'Send an email message to a recipient' },
  { name: 'weather_forecast', description: 'Get a weather forecast' },
];

const directMatch = routeTools(tools, 'send an email message', { maxTools: 3 });
assert.equal(directMatch.queryUsed, true);
assert.equal(directMatch.tools[0]?.name, 'send_email');

const chineseAliasMatch = routeTools(tools, '给客户发邮件', { maxTools: 3 });
assert.equal(chineseAliasMatch.queryUsed, true);
assert.equal(chineseAliasMatch.tools[0]?.name, 'send_email');

const unmatched = routeTools(tools, 'quantum avocado resonance', { maxTools: 6 });
assert.equal(unmatched.queryUsed, false);
assert.equal(unmatched.tools.length, 6);
assert.equal(unmatched.omitted, 2);
assert.deepEqual(
  unmatched.tools.map(tool => tool.name),
  tools.slice(0, 6).map(tool => tool.name),
);

console.log('Tool Router smoke tests passed.');
