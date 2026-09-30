import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { callMcp } from '../server/mcp.js';

const base = `http://127.0.0.1:${process.env.PORT || 3210}`;
const config = await fetch(`${base}/api/bootstrap`).then(response => response.json());
const expectedAdapter = process.argv[2] || 'unreal';
assert.equal(config.state.connected, true, 'Connect the scene first');
assert.equal(config.state.adapter, expectedAdapter, 'Wrong scene adapter');
const lab = { id: config.session_id, token: config.token };
const records = [];
async function call(name, args = {}, revision = config.state.revision) {
  const output = await callMcp(base, lab, name, args, revision);
  const data = JSON.parse(output.content[0].text);
  records.push({ name, args, isError: output.isError, result: data });
  return { output, data };
}
const { data: state } = await call('lab_get_state');
assert.equal(state.allowed_targets.length, 5);
for (const target_id of state.allowed_targets) {
  const { output, data } = await call('lab_highlight', { target_id, text: `MCP: ${target_id}` });
  assert.equal(output.isError, false);
  assert.equal(data.confirmed, true);
  assert.equal(data.adapter, expectedAdapter);
  assert.equal(data.target_id, target_id);
}
assert.equal((await call('lab_highlight', { target_id: 'unknown_actor', text: '' })).output.isError, true);
assert.equal((await call('lab_highlight', { target_id: 'cube_1', text: '' }, config.state.revision - 1)).data.error, 'STALE_CONTEXT');
assert.equal((await call('lab_clear_highlight')).data.confirmed, true);
const report = { time: new Date().toISOString(), adapter: expectedAdapter, passed: true, records };
await mkdir('.runtime', { recursive: true });
const path = `.runtime/${expectedAdapter}-mcp-acceptance.json`;
await writeFile(path, JSON.stringify(report, null, 2));
console.log(JSON.stringify({ passed: true, adapter: expectedAdapter, commands: records.length, report: path }));
