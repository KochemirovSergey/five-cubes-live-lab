import WebSocket, { WebSocketServer } from 'ws';
import { ToolLoop } from './tool-loop.js';
import { callMcp, liveTools } from './mcp.js';
import { scenario, liveInstructions, backendInstructions } from './scenario.js';

// The native client gets lab events/audio, never provider credentials or tool authority.
export function voiceGateway({ lab, apiKey, baseUrl, onDesktopExit, connectProvider = () => new WebSocket('wss://api.openai.com/v1/live/sessions', {
  headers: { Authorization: `Bearer ${apiKey}` }, handshakeTimeout: 15000,
}) }) {
  const sockets = new WebSocketServer({ noServer: true, maxPayload: 65536 });
  let owner;
  const sessions = new Set();
  sockets.on('connection', client => {
    let authenticated = false, upstream, loop, ready = false, closing = false, startTimer, closeTimer;
    let receivedAudio = 0, stopPromise, resolveStop;
    let contextTimer;
    const send = data => { if (client.readyState === WebSocket.OPEN) client.send(JSON.stringify(data)); };
    const sendUp = data => { if (upstream?.readyState === WebSocket.OPEN) upstream.send(JSON.stringify(data)); };
    const helloTimer = setTimeout(() => client.close(1008, 'HELLO_TIMEOUT'), 5000);
    const finish = confirmed => {
      clearTimeout(startTimer); clearTimeout(closeTimer); clearTimeout(contextTimer);
      ready = false; loop?.close();
      if (!confirmed && upstream && !closing) void cancel();
      const previous = upstream; upstream = null;
      previous?.removeAllListeners(); previous?.on('error', () => {}); previous?.terminate();
      closing = false;
      send({ type: 'voice.closed', finalized: confirmed });
      resolveStop?.(); resolveStop = null; stopPromise = null;
    };
    const cancel = async () => {
      lab.cancel();
      return callMcp(baseUrl(), lab, 'lab_clear_highlight', {}, lab.revision).catch(() => ({ isError: true }));
    };
    const stop = () => {
      if (!upstream) return Promise.resolve();
      if (closing) return stopPromise;
      stopPromise = new Promise(resolve => { resolveStop = resolve; });
      closing = true; ready = false; loop?.close(); clearTimeout(startTimer);
      void cancel(); sendUp({ type: 'session.close' });
      closeTimer = setTimeout(() => finish(false), 15000);
      return stopPromise;
    };
    sessions.add(stop);
    const append = (type, content) => sendUp({ type, content, delegation_id: null });
    const updateContext = () => {
      clearTimeout(contextTimer);
      append('session.thinking.append', JSON.stringify(lab.snapshot()));
    };
    const introduce = () => {
      updateContext();
      append('session.instructions.append', 'Открой разговор: произнеси только это общее вступление, без вызова инструментов, подсветки и подсказки первого шага. Затем жди просьбы пользователя. Вступление: ' + scenario.introduction);
    };
    const onLab = event => {
      send({ type: 'lab.state', state: lab.snapshot() });
      if (event.type === 'scene_disconnected' && upstream) { stop(); return; }
      if (!ready || closing) return;
      if (event.type === 'stage_changed') updateContext();
      else if (event.type === 'state') {
        clearTimeout(contextTimer);
        contextTimer = setTimeout(() => { if (ready && !closing) updateContext(); }, 250);
      }
    };
    const start = () => {
      if (upstream) return send({ type: 'voice.error', code: 'ALREADY_STARTED' });
      if (!apiKey) return send({ type: 'voice.error', code: 'API_KEY_MISSING' });
      if (!lab.snapshot().connected) return send({ type: 'voice.error', code: 'SCENE_NOT_READY' });
      receivedAudio = 0; closing = false;
      upstream = connectProvider();
      startTimer = setTimeout(() => { send({ type: 'voice.error', code: 'START_TIMEOUT' }); finish(false); }, 20000);
      loop = new ToolLoop({ send: sendUp, revision: () => lab.revision,
        execute: async (name, args, revision) => {
          if (closing || !ready) return { isError: true, content: [{ type: 'text', text: 'VOICE_CLOSED' }] };
          const output = await callMcp(baseUrl(), lab, name, args, revision);
          send({ type: 'tool.result', name, output }); return output;
        },
      });
      upstream.on('open', () => sendUp({ type: 'session.start', session: {
        model: process.env.OPENAI_LIVE_MODEL || 'gpt-live-1',
        instructions: liveInstructions,
        audio: { format: { type: 'audio/pcm', rate: 24000 }, output: { voice: 'marin' } },
        delegation: { type: 'responses', responses: {
          model: process.env.OPENAI_BACKEND_MODEL || 'gpt-6-sol', tools: liveTools, parallel_tool_calls: false,
          instructions: backendInstructions,
        } },
      } }));
      upstream.on('message', raw => {
        let e; try { e = JSON.parse(raw); } catch { send({ type: 'voice.error', code: 'PROVIDER_PROTOCOL' }); stop(); return; }
        if (e.type === 'session.started') { clearTimeout(startTimer); if (closing) return; ready = true; send({ type: 'voice.ready' }); introduce(); }
        else if (e.type === 'session.closed') { lab.log('voice_closed', { finalized: true, input_bytes: receivedAudio }); finish(true); return; }
        else if (e.type === 'error') { send({ type: 'voice.error', code: 'PROVIDER_ERROR' }); stop(); return; }
        if (closing) return;
        if (e.type === 'session.output_audio.delta') {
          if (client.bufferedAmount > 48000 * 2) { send({ type: 'voice.error', code: 'AUDIO_BACKPRESSURE' }); stop(); return; }
          if (client.readyState === WebSocket.OPEN) client.send(Buffer.from(e.delta, 'base64'));
        } else if (e.type === 'session.input_transcript.delta' || e.type === 'session.output_transcript.delta') {
          send({ type: 'voice.transcript', speaker: e.type.includes('.input_') ? 'user' : 'assistant', text: e.delta });
        }
        void loop?.event(e).catch(() => { send({ type: 'voice.error', code: 'TOOL_LOOP_FAILED' }); stop(); });
      });
      upstream.on('error', () => { send({ type: 'voice.error', code: 'PROVIDER_CONNECTION' }); finish(false); });
      upstream.on('unexpected-response', (_request, response) => { response.resume(); send({ type: 'voice.error', code: `PROVIDER_HTTP_${response.statusCode}` }); finish(false); });
      upstream.on('close', () => finish(false));
    };
    client.on('message', (raw, binary) => {
      try {
        if (binary) {
          if (!authenticated || !ready || closing) return;
          if (!raw.length || raw.length % 2 || raw.length > 9600) throw new Error('INVALID_PCM');
          if (upstream.bufferedAmount > 96000) { send({ type: 'voice.error', code: 'AUDIO_BACKPRESSURE' }); stop(); return; }
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
        else if (e.type === 'voice.stop') { if (upstream) stop(); else send({ type: 'voice.closed', finalized: true }); }
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
      stop();
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
