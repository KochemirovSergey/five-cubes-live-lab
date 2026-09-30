import { test } from 'node:test';
import assert from 'node:assert/strict';
import { once } from 'node:events';
import WebSocket from 'ws';
import { createApp } from '../server/index.js';
import { Lab, TARGETS } from '../server/lab.js';
import { callMcp } from '../server/mcp.js';
import { ToolLoop } from '../server/tool-loop.js';

import { stateFor } from './panel-state.js';
const body = output => JSON.parse(output.content[0].text);

test('real MCP HTTP → scene WebSocket → ACK, allowlist and stale context', async t => {
  const runtime = await createApp({ port: 0, apiKey: '' });
  t.after(() => runtime.close());
  const { lab, baseUrl } = runtime;
  const ws = new WebSocket(baseUrl.replace('http:', 'ws:') + '/scene');
  await once(ws, 'open');
  ws.send(JSON.stringify({ type: 'hello', session_id: lab.id, token: lab.token, adapter: 'unreal' }));
  await once(ws, 'message');
  ws.send(JSON.stringify({ type: 'state', state: stateFor(lab) }));
  await once(lab, 'event');
  let selected = null, commands = 0;
  ws.on('message', raw => {
    const command = JSON.parse(raw); commands++;
    selected = command.type === 'highlight' ? command.target_id : null;
    ws.send(JSON.stringify({ type: 'ack', session_id: lab.id, scene_version: 'scene-a', command_id: command.command_id, ok: true }));
  });
  const tools = (name, args = {}, revision = lab.revision) => callMcp(baseUrl, lab, name, args, revision);
  assert.deepEqual(body(await tools('lab_get_state')).allowed_targets, TARGETS);
  for (const [index, target_id] of TARGETS.slice(0,4).entries()) {
    lab.update(stateFor(lab, index));
    const output = await tools('lab_highlight', { target_id, text: 'Тест' });
    assert.equal(output.isError, false); assert.equal(body(output).confirmed, true); assert.equal(selected, target_id);
  }
  assert.equal((await tools('lab_highlight', { target_id: 'other_actor', text: 'test' })).isError, true);
  assert.equal(commands, 4);
  lab.update(stateFor(lab, 2));
  for (const target_id of TARGETS.slice(4)) {
    const output = await tools('lab_highlight', { target_id, text: 'Клавиша' });
    assert.equal(output.isError, false);
    assert.equal(selected, target_id);
  }
  lab.update(stateFor(lab, 3));
  assert.equal((await tools('lab_highlight', { target_id: 'panel.keypad.7', text: '' })).isError, false);
  lab.update(stateFor(lab, 4));
  assert.equal((await tools('lab_highlight', { target_id: 'panel.keypad.3', text: '' })).isError, false);
  const oldRevision = lab.revision;
  ws.send(JSON.stringify({ type: 'state', state: { ...stateFor(lab), scene_version: 'scene-b' } }));
  await once(lab, 'event');
  assert.equal(body(await tools('lab_highlight', { target_id: 'panel.rotary', text: '' }, oldRevision)).error, 'STALE_CONTEXT');
  const unauthorized = await fetch(`${baseUrl}/mcp`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: '{}' });
  assert.equal(unauthorized.status, 401);
  const foreign = await fetch(`${baseUrl}/api/bootstrap`, { headers: { Origin: 'https://example.com' } });
  assert.equal(foreign.status, 403);
});

test('Unreal Origin connects with pairing token; foreign Origin is rejected', async t => {
  const runtime = await createApp({ port: 0, apiKey: '' });
  t.after(() => runtime.close());
  const url = runtime.baseUrl.replace('http:', 'ws:') + '/scene';
  const foreign = new WebSocket(url, { origin: 'https://example.com' });
  await once(foreign, 'error');
  assert.equal(runtime.lab.adapter, null);
  const ws = new WebSocket(url, { origin: 'http://127.0.0.1' });
  await once(ws, 'open');
  ws.send(JSON.stringify({ type: 'hello', session_id: runtime.lab.id, token: 'wrong', adapter: 'unreal' }));
  const [code] = await once(ws, 'close');
  assert.equal(code, 1008);
  assert.equal(runtime.lab.adapter, null);
  const paired = new WebSocket(url, { origin: 'http://127.0.0.1' });
  await once(paired, 'open');
  paired.send(JSON.stringify({ type: 'hello', session_id: runtime.lab.id, token: runtime.lab.token, adapter: 'unreal' }));
  const [raw] = await once(paired, 'message');
  assert.equal(JSON.parse(raw).type, 'hello_ack');
});

test('missing ACK, cross-session ACK and disconnect never confirm success', async () => {
  const lab = new Lab({ timeoutMs: 35 });
  const other = new Lab();
  let command;
  const socket = { send: raw => { command = JSON.parse(raw); } };
  lab.attach(socket, 'unreal'); lab.update(stateFor(lab));
  const pending = lab.call('lab_highlight', { target_id: 'panel.rotary', text: 'two' });
  lab.ack(socket, { ...command, session_id: other.id, ok: true });
  assert.equal(body(await pending).error, 'ACK_TIMEOUT');
  const second = lab.call('lab_clear_highlight');
  lab.detach(socket);
  assert.equal(body(await second).error, 'SCENE_DISCONNECTED');
  assert.equal(body(await lab.call('lab_get_state')).error, 'SCENE_NOT_READY');
});

test('scene revision invalidates commands already in flight', async () => {
  const lab = new Lab(); const socket = { send() {} };
  lab.attach(socket, 'unreal'); lab.update(stateFor(lab));
  const pending = lab.call('lab_highlight', { target_id: 'panel.rotary', text: '' });
  lab.update({ ...stateFor(lab), scene_version: 'new' });
  assert.equal(body(await pending).error, 'STATE_CHANGED');
});

test('stop cancels pending operations and rejects late calls from old voice context', async () => {
  const lab = new Lab(); const socket = { send() {} };
  lab.attach(socket, 'unreal'); lab.update(stateFor(lab));
  const revision = lab.revision;
  const pending = lab.call('lab_highlight', { target_id: 'panel.rotary', text: '' });
  lab.cancel();
  assert.equal(body(await pending).error, 'CANCELLED');
  assert.equal(body(await lab.call('lab_highlight', { target_id: 'panel.rotary', text: '' }, revision)).error, 'STALE_CONTEXT');
});

test('Live function loop collects output items, deduplicates, returns all results then continues', async () => {
  const sent = [], executed = [];
  const loop = new ToolLoop({ send: e => sent.push(e), revision: () => 7,
    execute: async (name, args, revision, call_id) => { executed.push({ name, args, revision, call_id }); return { confirmed: true }; } });
  const event = e => loop.event({ type: 'response.event', delegation_id: 'del-1', event: e });
  await event({ type: 'response.created', response: { id: 'r1' } });
  const item = { type: 'function_call', name: 'lab_highlight', call_id: 'c1', arguments: '{"target_id":"panel.rotary","text":"Ручка"}' };
  await event({ type: 'response.output_item.done', item });
  await event({ type: 'response.output_item.done', item });
  assert.equal(sent.length, 0);
  await event({ type: 'response.completed', response: { id: 'r1', output: [] } });
  assert.equal(executed.length, 1); assert.equal(executed[0].revision, 7);
  assert.deepEqual(sent.map(e => e.type), ['response.item.create', 'response.create']);
  assert.equal(sent[0].item.call_id, 'c1');
});

test('telemetry keeps valid commands; all targets survive stages but stale commands fail', async () => {
  const lab=new Lab();let command;
  const socket={send: raw=>{command=JSON.parse(raw)}};lab.attach(socket,'unreal');lab.update(stateFor(lab));
  const revision=lab.revision;
  const future=lab.call('lab_highlight',{target_id:'panel.slider',text:''});
  lab.ack(socket,{...command,ok:true});assert.equal((await future).isError,false);
  const pending=lab.call('lab_highlight',{target_id:'panel.rotary',text:''});
  lab.update(stateFor(lab,0,{instrument_state:{...lab.state.instrument_state,rotary_angle:2}}));
  assert.equal(lab.revision,revision);lab.ack(socket,{...command,ok:true});assert.equal((await pending).isError,false);
  const stale=lab.call('lab_highlight',{target_id:'panel.rotary',text:''});lab.update(stateFor(lab,1));
  assert.equal(body(await stale).error,'STATE_CHANGED');
  assert.equal(body(await lab.call('lab_highlight',{target_id:'panel.slider',text:''},revision)).error,'STALE_CONTEXT');
  const seq=lab.state.state_seq;lab.update(stateFor(lab,0,{state_seq:seq-1}));assert.equal(lab.state.stage,2);
  lab.update(stateFor(lab,4));assert.deepEqual(lab.snapshot().allowed_targets,TARGETS);
  lab.update(stateFor(lab,0,{scene_version:'reset',state_seq:1}));assert.equal(lab.state.stage,1);
});
