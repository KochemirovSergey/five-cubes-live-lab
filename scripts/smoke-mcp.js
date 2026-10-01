import assert from 'node:assert/strict';
import { mkdir, writeFile, readFile } from 'node:fs/promises';
import { laboratory } from '../server/laboratories.js';
import { callMcp } from '../server/mcp.js';

const connection=process.env.LAB_CONNECTION_FILE ? JSON.parse(await readFile(process.env.LAB_CONNECTION_FILE,'utf8')) : null;
const base = connection ? connection.url.replace('ws:','http:').replace(/\/scene$/,'') : `http://127.0.0.1:${process.env.PORT || 3210}`;
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
assert.deepEqual(state.allowed_targets, laboratory(state.lab_id, state.mode).targets.map(t => t.id));
for (const target_id of state.allowed_targets) {
  const { output, data } = await call('lab_highlight', { target_id, text: `MCP: ${target_id}` });
  if(state.mode==='full' && state.instrument_state.visible_targets && !state.instrument_state.visible_targets.includes(target_id)){assert.equal(output.isError,true);assert.match(data.error,/TARGET_HIDDEN/);continue;}
  assert.equal(output.isError, false);
  assert.equal(data.confirmed, true);
  assert.equal(data.adapter, expectedAdapter);
  assert.equal(data.target_id, target_id);
}
assert.equal((await call('lab_highlight', { target_id: 'unknown_actor', text: '' })).output.isError, true);
assert.equal((await call('lab_highlight', { target_id: config.state.allowed_targets[0] || 'panel.rotary', text: '' }, config.state.revision - 1)).data.error, 'STALE_CONTEXT');
const samples=records.filter(r=>r.name==='lab_highlight'&&!r.isError).map(r=>r.result.latency_ms).sort((a,b)=>a-b);
assert.equal((await call('lab_clear_highlight')).data.confirmed, true);
const report = { timing_scope:'MCP command sent to Unreal ACK, excludes voice and HTTP setup', latency:{n:samples.length,median_ms:samples[Math.floor(samples.length/2)],p95_ms:samples[Math.ceil(samples.length*.95)-1]}, time: new Date().toISOString(), adapter: expectedAdapter, passed: true, records };
await mkdir('.runtime', { recursive: true });
const path = `.runtime/${state.lab_id}${state.mode ? `-${state.mode}` : ''}-${expectedAdapter}-mcp-acceptance.json`;
await writeFile(path, JSON.stringify(report, null, 2));
console.log(JSON.stringify({ passed: true, adapter: expectedAdapter, commands: records.length, report: path }));
