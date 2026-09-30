import { test } from 'node:test';
import assert from 'node:assert/strict';
import { EventEmitter, once } from 'node:events';
import WebSocket from 'ws';
import { createApp } from '../server/index.js';
import { TARGETS } from '../server/lab.js';
class Provider extends EventEmitter {
  readyState = WebSocket.OPEN; bufferedAmount = 0; sent = [];
  send(raw) {
    const e = JSON.parse(raw); this.sent.push(e);
    if (e.type === 'session.start') queueMicrotask(() => this.emit('message', JSON.stringify({ type: 'session.started' })));
    if (e.type === 'session.close') queueMicrotask(() => this.emit('message', JSON.stringify({ type: 'session.closed' })));
  }
  terminate() { this.readyState = WebSocket.CLOSED; }
  event(e) { this.emit('message', JSON.stringify(e)); }
}
async function fixture(t) {
  const providers = [];
  const runtime = await createApp({ port: 0, apiKey: 'fake-test-key', connectProvider: () => {
    const p = new Provider(); providers.push(p); queueMicrotask(() => p.emit('open')); return p;
  } });
  t.after(() => runtime.close());
  const { lab } = runtime;
  const adapter = { send: raw => { const c = JSON.parse(raw); queueMicrotask(() => lab.ack(adapter, { ...c, ok: true })); } };
  lab.attach(adapter, 'unreal');
  lab.update({ session_id: lab.id, scene_version: 'v1', stage: 1, stages_total: 1, stage_id: 'five_cubes', allowed_targets: TARGETS, completed: [] });
  const client = new WebSocket(runtime.baseUrl.replace('http:', 'ws:') + '/voice');
  const messages = []; client.on('message', (raw, binary) => messages.push(binary ? Buffer.from(raw) : JSON.parse(raw)));
  await once(client, 'open');
  client.send(JSON.stringify({ type: 'hello', session_id: lab.id, token: lab.token }));
  await once(client, 'message');
  return { ...runtime, client, providers, messages, send: e => client.send(JSON.stringify(e)) };
}
const waitFor = async predicate => { for (let i = 0; i < 100; i++) { if (predicate()) return; await new Promise(r => setTimeout(r, 5)); } assert.fail('event timeout'); };
test('native voice PCM round trip, confirmed MCP call and graceful stop', async t => {
  const f = await fixture(t);
  assert.equal((await fetch(f.baseUrl)).status, 404);
  f.send({ type: 'voice.start' }); await waitFor(() => f.messages.some(e => e.type === 'voice.ready'));
  const p = f.providers[0]; assert.equal(p.sent[0].session.model, 'gpt-live-1');
  f.client.send(Buffer.alloc(960)); await waitFor(() => p.sent.some(e => e.type === 'session.input_audio.append'));
  assert.equal(Buffer.from(p.sent.at(-1).audio, 'base64').length, 960);
  p.event({ type: 'session.output_audio.delta', delta: Buffer.alloc(960, 1).toString('base64') });
  await waitFor(() => f.messages.some(Buffer.isBuffer));
  const event = e => p.event({ type: 'response.event', delegation_id: 'd1', event: e });
  event({ type: 'response.created', response: { id: 'r1' } });
  event({ type: 'response.output_item.done', item: { type: 'function_call', call_id: 'call1', name: 'lab_highlight', arguments: JSON.stringify({ target_id: 'cube_2', text: 'Второй' }) } });
  event({ type: 'response.completed' });
  await waitFor(() => p.sent.some(e => e.type === 'response.create'));
  assert.equal(JSON.parse(p.sent.find(e => e.type === 'response.item.create').item.output).isError, false);
  f.send({ type: 'voice.stop' }); await waitFor(() => f.messages.some(e => e.type === 'voice.closed'));
  assert.equal(f.messages.find(e => e.type === 'voice.closed').finalized, true);
  assert.equal(p.readyState, WebSocket.CLOSED);
});
test('native rejects duplicate start, odd PCM and finalizes after client loss', async t => {
  const f = await fixture(t); f.send({ type: 'voice.start' }); await waitFor(() => f.messages.some(e => e.type === 'voice.ready'));
  f.send({ type: 'voice.start' }); await waitFor(() => f.messages.some(e => e.code === 'ALREADY_STARTED'));
  assert.equal(f.providers.length, 1);
  f.client.send(Buffer.alloc(3)); const [code] = await once(f.client, 'close'); assert.equal(code, 1008);
  await waitFor(() => f.providers[0].readyState === WebSocket.CLOSED);
});
test('native refuses wrong token and second controller', async t => {
  const f = await fixture(t);
  for (const token of ['wrong', f.lab.token]) {
    const c = new WebSocket(f.baseUrl.replace('http:', 'ws:') + '/voice'); await once(c, 'open');
    c.send(JSON.stringify({ type: 'hello', session_id: f.lab.id, token }));
    assert.equal((await once(c, 'close'))[0], 1008);
  }
});
test('native provider backpressure stops session, no PCM accumulates', async t => {
  const f = await fixture(t); f.send({ type: 'voice.start' }); await waitFor(() => f.messages.some(e => e.type === 'voice.ready'));
  f.providers[0].bufferedAmount = 96001; f.client.send(Buffer.alloc(960));
  await waitFor(() => f.messages.some(e => e.type === 'voice.closed'));
  assert.ok(f.messages.some(e => e.code === 'AUDIO_BACKPRESSURE'));
  assert.equal(f.providers[0].sent.filter(e => e.type === 'session.input_audio.append').length, 0);
});
