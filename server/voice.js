import WebSocket, { WebSocketServer } from 'ws';
import { ToolLoop } from './tool-loop.js';
import { callMcp, liveToolsFor } from './mcp.js';
import { contextSignature } from './oberbeck-state.js';
import { instructionsFor } from './scenario.js';
import { liveContext } from './live-context.js';

// Two seconds of transport headroom: raw output PCM is 48 kB/s; input JSON/base64 is larger.
// A transient burst is not a failed conversation; sustained stalls still terminate explicitly.
// The native client gets lab events/audio, never provider credentials or tool authority.
export function voiceGateway({ lab, apiKey, baseUrl, onDesktopExit, connectProvider = () => new WebSocket('wss://api.openai.com/v1/live/sessions', {
  headers: { Authorization: `Bearer ${apiKey}` }, handshakeTimeout: 15000,
}) }) {
  const sockets = new WebSocketServer({ noServer: true, maxPayload: 65536 });
  let owner;
  const sessions = new Set();
  sockets.on('connection', client => {
    let authenticated = false, upstream, loop, ready = false, closing = false, startTimer, closeTimer;
    let receivedAudio = 0, stopPromise, resolveStop, stopReason, startedAt;
    let contextTimer, activeSpec, activeLab, activeMode, lastContext = '', lastContextAt = 0;
    const send = data => { if (client.readyState === WebSocket.OPEN) client.send(JSON.stringify(data)); };
    const sendUp = data => { if (upstream?.readyState === WebSocket.OPEN) upstream.send(JSON.stringify(data)); };
    const helloTimer = setTimeout(() => client.close(1008, 'HELLO_TIMEOUT'), 5000);
    const finish = confirmed => {
      if (!upstream) return;
      const reason = stopReason || (confirmed ? 'provider_closed' : 'provider_disconnected');
      clearTimeout(startTimer); clearTimeout(closeTimer); clearTimeout(contextTimer);
      ready = false; loop?.close();
      if (!confirmed && upstream && !closing) void cancel();
      send({type:'voice.reset_output'});
      const previous = upstream; upstream = null;
      previous?.removeAllListeners(); previous?.on('error', () => {}); previous?.terminate();
      closing = false;
      lab.log('voice_closed', { finalized: confirmed, reason, input_bytes: receivedAudio, duration_ms: startedAt ? Date.now()-startedAt : 0 });
      send({ type: 'voice.closed', finalized: confirmed, reason });
      resolveStop?.(); resolveStop = null; stopPromise = null;
    };
    const cancel = async () => {
      lab.cancel();
      return callMcp(baseUrl(), lab, 'lab_clear_highlight', {}, lab.revision).catch(() => ({ isError: true }));
    };
    const stop = (reason = 'server_shutdown') => {
      if (!upstream) return Promise.resolve();
      if (closing) return stopPromise;
      stopReason ||= reason;
      stopPromise = new Promise(resolve => { resolveStop = resolve; });
      closing = true; ready = false;send({type:'voice.reset_output'}); loop?.close(); clearTimeout(startTimer);
      lab.log('voice_stop_requested', { reason: stopReason });
      void cancel(); sendUp({ type: 'session.close' });
      closeTimer = setTimeout(() => finish(false), 15000);
      return stopPromise;
    };
    sessions.add(stop);
    const append = (type, content) => sendUp({ type, content, delegation_id: null });
    const updateContext = () => {
      clearTimeout(contextTimer); contextTimer = null;
      lastContext=contextSignature(lab.state);lastContextAt=Date.now();
      append('session.thinking.append', liveContext(lab.snapshot()));
    };
    const introduce = () => {
      updateContext();
      append('session.instructions.append', 'Открой разговор: произнеси только это общее вступление, без вызова инструментов, подсветки и подсказки первого шага. Затем жди просьбы пользователя. Вступление: ' + activeSpec.scenario.introduction);
    };
    const onLab = event => {
      send({ type: 'lab.state', state: lab.snapshot() });
      if ((event.type === 'scene_disconnected' || (activeLab && (lab.state?.lab_id !== activeLab || lab.state?.mode !== activeMode))) && upstream) { stop(event.type === 'scene_disconnected' ? 'scene_disconnected' : 'scene_changed'); return; }
      if (!ready || closing) return;
      if (event.type === 'stage_changed' || (event.type === 'state' && contextSignature(lab.state)!==lastContext)) updateContext();
      else if (event.type === 'state' && !contextTimer && lab.state?.instrument_state?.timer_running) {
        contextTimer=setTimeout(()=>{contextTimer=null;if(ready&&!closing)updateContext();},Math.max(0,2000-(Date.now()-lastContextAt)));
      }
    };
    const start = () => {
      if (upstream) return send({ type: 'voice.error', code: 'ALREADY_STARTED' });
      if (!apiKey) return send({ type: 'voice.error', code: 'API_KEY_MISSING' });
      if (!lab.snapshot().connected) return send({ type: 'voice.error', code: 'SCENE_NOT_READY' });
      activeLab = lab.state.lab_id; activeMode = lab.state.mode; activeSpec = instructionsFor(activeLab, activeMode);
      receivedAudio = 0; closing = false; stopReason = null; startedAt = Date.now();
      lab.log('voice_start_requested');
      upstream = connectProvider();
      startTimer = setTimeout(() => { send({ type: 'voice.error', code: 'START_TIMEOUT' }); stopReason='start_timeout'; finish(false); }, 20000);
      loop = new ToolLoop({ send: sendUp, revision: () => lab.revision,
        execute: async (name, args, revision) => {
          if (closing || !ready) return { isError: true, content: [{ type: 'text', text: 'VOICE_CLOSED' }] };
          const output = await callMcp(baseUrl(), lab, name, args, revision);
          send({ type: 'tool.result', name, output }); return output;
        },
      });
      upstream.on('open', () => sendUp({ type: 'session.start', session: {
        model: process.env.OPENAI_LIVE_MODEL || 'gpt-live-1',
        instructions: activeSpec.liveInstructions,
        audio: { format: { type: 'audio/pcm', rate: 24000 }, output: { voice: 'marin' } },
        delegation: { type: 'responses', responses: {
          model: process.env.OPENAI_BACKEND_MODEL || 'gpt-6-sol', tools: liveToolsFor(activeLab, activeMode), parallel_tool_calls: false,
          instructions: activeSpec.backendInstructions,
        } },
      } }));
      upstream.on('message', raw => {
        let e; try { e = JSON.parse(raw); } catch { send({ type: 'voice.error', code: 'PROVIDER_PROTOCOL' }); stop('provider_protocol'); return; }
        if (e.type === 'session.started') { clearTimeout(startTimer); if (closing) return; ready = true; send({ type: 'voice.ready' }); introduce(); }
        else if (e.type === 'session.closed') { finish(true); return; }
        else if (e.type === 'error') {
          const error = e.error || e;
          // Log structured identifiers only: provider messages may echo user data.
          const identifier = value => typeof value === 'string' && /^[a-zA-Z0-9_.-]{1,100}$/.test(value) ? value : 'unknown';
          lab.log('voice_provider_error', { provider_code: identifier(error.code), parameter: identifier(error.param) });
          const contextLimit = error.param === 'content' && /500 tokens/.test(error.message || '');
          send({ type: 'voice.error', code: contextLimit ? 'PROVIDER_CONTEXT_LIMIT' : `PROVIDER_ERROR_${identifier(error.code)}` });
          stop('provider_error'); return;
        }
        if (closing) return;
        if (e.type === 'session.output_audio.delta') {
          if (client.bufferedAmount > 96000) { send({ type: 'voice.error', code: 'AUDIO_BACKPRESSURE' }); stop('output_backpressure'); return; }
          if (client.readyState === WebSocket.OPEN) client.send(Buffer.from(e.delta, 'base64'));
        } else if (e.type === 'session.input_transcript.delta' || e.type === 'session.output_transcript.delta') {
          send({ type: 'voice.transcript', speaker: e.type.includes('.input_') ? 'user' : 'assistant', text: e.delta });
        }
        void loop?.event(e).catch(() => { send({ type: 'voice.error', code: 'TOOL_LOOP_FAILED' }); stop('tool_loop_failed'); });
      });
      upstream.on('error', () => { send({ type: 'voice.error', code: 'PROVIDER_CONNECTION' }); stopReason='provider_connection'; finish(false); });
      upstream.on('unexpected-response', (_request, response) => { response.resume(); send({ type: 'voice.error', code: `PROVIDER_HTTP_${response.statusCode}` }); stopReason=`provider_http_${response.statusCode}`; finish(false); });
      upstream.on('close', () => finish(false));
    };
    client.on('message', (raw, binary) => {
      try {
        if (binary) {
          if (!authenticated || !ready || closing) return;
          if (!raw.length || raw.length % 2 || raw.length > 9600) throw new Error('INVALID_PCM');
          if (upstream.bufferedAmount > 144000) { send({ type: 'voice.error', code: 'AUDIO_BACKPRESSURE' }); stop('input_backpressure'); return; }
          receivedAudio += raw.length;
          sendUp({ type: 'session.input_audio.append', audio: raw.toString('base64') }); return;
        }
        const e = JSON.parse(raw);
        if (!authenticated) {
          if (e.type !== 'hello' || e.session_id !== lab.id || e.token !== lab.token || owner) throw new Error('INVALID_HELLO_OR_BUSY');
          authenticated = true; owner = client; clearTimeout(helloTimer); lab.on('event', onLab);
          send({ type: 'hello_ack', state: lab.snapshot(), key_configured: Boolean(apiKey) }); return;
        }
        if (e.type === 'voice.start') start();
        else if (e.type === 'voice.stop') { if (upstream) stop(['user','scene_changed','app_exit','output_backpressure','invalid_output_pcm','capture_overflow','capture_timeout'].includes(e.reason) ? e.reason : 'client_stop'); else send({ type: 'voice.closed', finalized: true }); }
        else if(e.type==='voice.metric'){
          if(!['speech_end_to_highlight_ack','interruption_renderer'].includes(e.kind)||!Number.isFinite(e.milliseconds)||e.milliseconds< -1||e.milliseconds>30000||typeof e.success!=='boolean')throw new Error('INVALID_METRIC');
          lab.log('voice_metric',{kind:e.kind,milliseconds:e.milliseconds,success:e.success,measurement:'native_energy_detector_not_acoustic'});
        }
        else if (e.type === 'desktop.shutdown' && onDesktopExit) void onDesktopExit();
        else if (e.type === 'lab.command') {
          if (!['lab_highlight', 'lab_clear_highlight'].includes(e.name)) throw new Error('UNKNOWN_TOOL');
          void callMcp(baseUrl(), lab, e.name, e.args || {}, lab.revision)
            .then(output => send({ type: 'tool.result', name: e.name, output }))
            .catch(() => send({ type: 'voice.error', code: 'MCP_TRANSPORT_FAILED' }));
        } else throw new Error('UNKNOWN_MESSAGE');
      } catch { client.close(1008, 'INVALID_MESSAGE'); }
    });
    client.on('error', () => {});
    client.on('close', () => {
      clearTimeout(helloTimer); lab.off('event', onLab); sessions.delete(stop);
      if (owner === client) owner = null;
      stop('client_disconnected');
    });
  });
  return { sockets, close: async () => {
    await Promise.all([...sessions].map(stop => stop()));
    for (const client of sockets.clients) client.close(1001, 'SERVER_SHUTDOWN');
    await new Promise(resolve => setTimeout(resolve, 200));
    for (const client of sockets.clients) client.terminate();
    sockets.close();
  } };
}
