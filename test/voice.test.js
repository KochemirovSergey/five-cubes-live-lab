import { test } from 'node:test';
import assert from 'node:assert/strict';
import { EventEmitter, once } from 'node:events';
import WebSocket from 'ws';
import { createApp } from '../server/index.js';
import { stateFor } from './panel-state.js';
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
  lab.update(stateFor(lab));
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
  event({ type: 'response.output_item.done', item: { type: 'function_call', call_id: 'call1', name: 'lab_highlight', arguments: JSON.stringify({ target_id: 'panel.rotary', text: 'Второй' }) } });
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
  f.providers[0].bufferedAmount = 144001; f.client.send(Buffer.alloc(960));
  await waitFor(() => f.messages.some(e => e.type === 'voice.closed'));
  assert.ok(f.messages.some(e => e.code === 'AUDIO_BACKPRESSURE'));
  assert.equal(f.providers[0].sent.filter(e => e.type === 'session.input_audio.append').length, 0);
});

test('intro and stage updates never trigger highlights or automatic stage speech', async t => {
  const f = await fixture(t); f.send({ type: 'voice.start' });
  await waitFor(() => f.providers[0]?.sent.some(e => e.type === 'session.instructions.append'));
  const p = f.providers[0];
  assert.ok(p.sent.some(e => e.content?.includes('Открой разговор')));
  assert.equal(f.lab.events.filter(e=>e.type==='tool_call').length,0);
  // A user-requested tool delegation selects a future-stage control.
  const event = e => p.event({ type: 'response.event', delegation_id: 'requested', event: e });
  event({ type: 'response.created', response: { id: 'request1' } });
  event({ type: 'response.output_item.done', item: { type: 'function_call', call_id: 'highlight1', name: 'lab_highlight', arguments: JSON.stringify({ target_id: 'panel.toggle', text: 'Рычаг' }) } });
  event({ type: 'response.completed' });
  await waitFor(() => f.lab.events.some(e=>e.type==='tool_result'));
  const callsBefore=f.lab.events.filter(e=>e.type==='tool_call').length;
  for(const index of [1,2,3,4])f.lab.update(stateFor(f.lab,index));
  await waitFor(() => p.sent.some(e=>e.type==='session.thinking.append' && e.content.includes('"finished":true')));
  assert.equal(f.providers.length,1);
  assert.equal(p.sent.filter(e=>e.type==='session.commentary.append').length,0);
  assert.equal(p.sent.filter(e=>e.type==='session.instructions.append').length,1);
  assert.equal(f.lab.events.filter(e=>e.type==='tool_call').length,callsBefore);
  assert.equal(f.messages.filter(e=>e.type==='voice.reset_output').length,0);
  f.send({type:'voice.stop'});await waitFor(()=>f.messages.some(e=>e.type==='voice.closed'));
  await waitFor(()=>f.lab.events.some(e=>e.type==='tool_call'&&e.name==='lab_clear_highlight'));
});

test('switching laboratories closes old voice; new session gets Oberbeck prompt and targets', async t => {
  const f=await fixture(t);f.send({type:'voice.start'});
  await waitFor(()=>f.messages.some(e=>e.type==='voice.ready'));
  const {laboratory}=await import('../server/laboratories.js');
  f.lab.update({lab_id:'oberbeck',session_id:f.lab.id,scene_version:'ob',state_seq:1,stage:1,stages_total:2,stage_id:'first',finished:false,completed:[],allowed_targets:laboratory('oberbeck').targets.map(t=>t.id),instrument_state:{phase:'preparing',pulley:0,radius:.12,height:0,current_height:0,angle:0,stopwatch:0,timer_running:false,paused:false,records:[]}});
  await waitFor(()=>f.messages.some(e=>e.type==='voice.closed'));
  f.send({type:'voice.start'});await waitFor(()=>f.providers.length===2&&f.providers[1].sent.some(e=>e.type==='session.instructions.append'));
  const started=f.providers[1].sent.find(e=>e.type==='session.start');
  assert.match(started.session.instructions,/Обербека/);
  assert.ok(started.session.delegation.responses.tools.find(t=>t.name==='lab_highlight').parameters.properties.target_id.enum.every(id=>id.startsWith('oberbeck.')));
  assert.equal(f.providers[1].sent.filter(e=>e.type==='session.instructions.append').length,1);
});

test('Oberbeck mode change restarts voice context without automatic highlighting', async t => {
 const f=await fixture(t);const {laboratory}=await import('../server/laboratories.js');
 f.lab.update({lab_id:'oberbeck',mode:'intro',session_id:f.lab.id,scene_version:'intro',state_seq:1,stage:1,stages_total:2,stage_id:'first',finished:false,completed:[],allowed_targets:laboratory('oberbeck').targets.map(t=>t.id),instrument_state:{phase:'preparing',pulley:0,radius:.12,height:0,current_height:0,angle:0,stopwatch:0,timer_running:false,paused:false,records:[]}});
 f.send({type:'voice.start'});await waitFor(()=>f.messages.some(e=>e.type==='voice.ready'));
 f.lab.update({lab_id:'oberbeck',mode:'full',session_id:f.lab.id,scene_version:'full',state_seq:1,stage:1,stages_total:5,stage_id:'unloaded',finished:false,completed:[],allowed_targets:laboratory('oberbeck','full').targets.map(t=>t.id),instrument_state:{phase:'preparing',pulley:0,mass_count:4,balanced:true,stopwatch:0,timer_running:false,paused:false,substep:'prepare_or_calculate',measurements:{},series:[],checked_answers:[]}});
 await waitFor(()=>f.messages.some(e=>e.type==='voice.closed'));
 f.send({type:'voice.start'});await waitFor(()=>f.providers.length===2&&f.providers[1].sent.some(e=>e.type==='session.instructions.append'));
 const p=f.providers[1];const session=p.sent.find(e=>e.type==='session.start').session;
 assert.match(session.instructions,/полной лабораторной/);assert.ok(session.delegation.responses.tools.find(t=>t.name==='lab_highlight').parameters.properties.target_id.enum.includes('oberbeck.caliper'));
 assert.ok(!p.sent.find(e=>e.type==='session.instructions.append').content.includes('undefined'));
 assert.equal(f.lab.events.filter(e=>e.type==='tool_call'&&e.name==='lab_highlight').length,0);
});

test('continuous timer telemetry is coalesced and context changes are immediate and silent',async t=>{
 const f=await fixture(t);const {laboratory}=await import('../server/laboratories.js');
 let seq=1;const state={lab_id:'oberbeck',mode:'full',session_id:f.lab.id,scene_version:'full',state_seq:seq,stage:1,stages_total:5,stage_id:'unloaded',completed:[],allowed_targets:laboratory('oberbeck','full').targets.map(t=>t.id),instrument_state:{phase:'falling',pulley:0,mass_count:0,balanced:true,stopwatch:0,timer_running:true,paused:false,substep:'wait_for_landing',measurements:{},series:[],checked_answers:[]}};
 f.lab.update(state);f.send({type:'voice.start'});await waitFor(()=>f.providers[0]?.sent.some(e=>e.type==='session.instructions.append'));
 const p=f.providers[0],before=p.sent.filter(e=>e.type==='session.thinking.append').length;
 for(let i=1;i<=30;i++){state.state_seq=++seq;state.instrument_state.stopwatch=i/10;f.lab.update(state);}
 assert.equal(p.sent.filter(e=>e.type==='session.thinking.append').length,before);
 state.state_seq=++seq;state.instrument_state.phase='landed';state.instrument_state.substep='stop_timer_after_landing';f.lab.update(state);
 assert.equal(p.sent.filter(e=>e.type==='session.thinking.append').length,before+1);
 assert.equal(p.sent.filter(e=>e.type==='session.instructions.append').length,1);assert.equal(f.lab.pending.size,0);
});

test('provider loss clears queued native audio and stops further old audio',async t=>{
 const f=await fixture(t);f.send({type:'voice.start'});await waitFor(()=>f.messages.some(e=>e.type==='voice.ready'));
 const p=f.providers[0];p.event({type:'session.output_audio.delta',delta:Buffer.alloc(960).toString('base64')});await waitFor(()=>f.messages.some(Buffer.isBuffer));
 p.emit('error',new Error('simulated disconnect'));await waitFor(()=>f.messages.some(e=>e.type==='voice.closed'));
 assert.ok(f.messages.some(e=>e.type==='voice.reset_output'));const n=f.messages.filter(Buffer.isBuffer).length;
 p.event({type:'session.output_audio.delta',delta:Buffer.alloc(960).toString('base64')});assert.equal(f.messages.filter(Buffer.isBuffer).length,n);
});

test('provider validation error retains safe diagnostics and allows a fresh start',async t=>{
 const f=await fixture(t);f.send({type:'voice.start'});await waitFor(()=>f.messages.some(e=>e.type==='voice.ready'));
 f.providers[0].event({type:'error',error:{code:'invalid_value',param:'content',message:'Context append text must not exceed 500 tokens. fake-test-key'}});
 await waitFor(()=>f.messages.some(e=>e.type==='voice.closed'));
 assert.ok(f.messages.some(e=>e.code==='PROVIDER_CONTEXT_LIMIT'));
 const diagnostic=f.lab.events.find(e=>e.type==='voice_provider_error');
 assert.equal(diagnostic.provider_code,'invalid_value');assert.equal(diagnostic.parameter,'content');
 assert.ok(!JSON.stringify(diagnostic).includes('fake-test-key'));
 f.send({type:'voice.start'});await waitFor(()=>f.providers.length===2);
 await waitFor(()=>f.messages.filter(e=>e.type==='voice.ready').length===2);
});


test('short transport bursts keep conversation open; sustained backlog records its cause',async t=>{
 const f=await fixture(t);f.send({type:'voice.start'});await waitFor(()=>f.messages.some(e=>e.type==='voice.ready'));
 const p=f.providers[0];p.bufferedAmount=40000;f.client.send(Buffer.alloc(960));
 await waitFor(()=>p.sent.some(e=>e.type==='session.input_audio.append'));
 assert.equal(f.messages.filter(e=>e.type==='voice.closed').length,0);
 p.bufferedAmount=144001;f.client.send(Buffer.alloc(960));
 await waitFor(()=>f.messages.some(e=>e.type==='voice.closed'));
 assert.equal(f.messages.find(e=>e.type==='voice.closed').reason,'input_backpressure');
 assert.equal(f.lab.events.find(e=>e.type==='voice_closed').reason,'input_backpressure');
});

test('native stop reason survives provider finalization and resets for next session',async t=>{
 const f=await fixture(t);f.send({type:'voice.start'});await waitFor(()=>f.messages.some(e=>e.type==='voice.ready'));
 f.send({type:'voice.stop',reason:'capture_timeout'});await waitFor(()=>f.messages.some(e=>e.type==='voice.closed'));
 assert.equal(f.messages.find(e=>e.type==='voice.closed').reason,'capture_timeout');
 f.send({type:'voice.start'});await waitFor(()=>f.messages.filter(e=>e.type==='voice.ready').length===2);
 f.providers[1].event({type:'session.closed'});await waitFor(()=>f.messages.filter(e=>e.type==='voice.closed').length===2);
 assert.equal(f.messages.filter(e=>e.type==='voice.closed')[1].reason,'provider_closed');
});
