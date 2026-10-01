// Explicit, paid API smoke test. Run manually: npm run smoke:live.
// Uses a typed request, not a microphone, and the currently connected real scene.
import WebSocket from 'ws';
import { ToolLoop } from '../server/tool-loop.js';
import { liveToolsFor } from '../server/mcp.js';
import { instructionsFor } from '../server/scenario.js';
import { readFile } from 'node:fs/promises';

let inputAudio;
const audioPath = process.argv[2];
if (audioPath) {
  const wav = await readFile(audioPath);
  if (wav.toString('ascii', 0, 4) !== 'RIFF' || wav.toString('ascii', 8, 12) !== 'WAVE') throw new Error('Expected PCM16 mono 24kHz WAV');
  let validFormat = false;
  for (let offset = 12; offset + 8 <= wav.length;) {
    const size = wav.readUInt32LE(offset + 4), type = wav.toString('ascii', offset, offset + 4);
    if (type === 'fmt ') validFormat = wav.readUInt16LE(offset + 8) === 1 && wav.readUInt16LE(offset + 10) === 1 && wav.readUInt32LE(offset + 12) === 24000 && wav.readUInt16LE(offset + 22) === 16;
    if (type === 'data') inputAudio = wav.subarray(offset + 8, offset + 8 + size);
    offset += 8 + size + (size % 2);
  }
  if (!validFormat || !inputAudio) throw new Error('Expected PCM16 mono 24kHz WAV');
}

if (!process.env.OPENAI_API_KEY) throw new Error('Set OPENAI_API_KEY in .env');
const connection=process.env.LAB_CONNECTION_FILE ? JSON.parse(await readFile(process.env.LAB_CONNECTION_FILE,'utf8')) : null;
const base = connection ? connection.url.replace('ws:','http:').replace(/\/scene$/,'') : `http://127.0.0.1:${process.env.PORT || 3210}`;
const config = await fetch(`${base}/api/bootstrap`).then(r => r.json());
if (!config.state.connected) throw new Error('Connect the Unreal scene first');
const { liveInstructions, backendInstructions } = instructionsFor(config.state.lab_id, config.state.mode);
const liveTools = liveToolsFor(config.state.lab_id, config.state.mode);
const headers = { Authorization: `Bearer ${config.token}`, 'x-lab-session': config.session_id, 'Content-Type': 'application/json' };
const socket = new WebSocket('wss://api.openai.com/v1/live/sessions', {
  headers: { Authorization: `Bearer ${process.env.OPENAI_API_KEY}` }, handshakeTimeout: 15000,
});
let started = false, confirmed = false, finalized = false, audioBytes = 0, closing = false, backendAnswered = false;
let audioTimer;
const eventTypes = new Set();
const send = event => { if (socket.readyState === WebSocket.OPEN) socket.send(JSON.stringify(event)); };
const close = () => { if (closing) return; closing = true; loop.close(); send({ type: 'session.close' }); };
const loop = new ToolLoop({ send, revision: () => config.state.revision,
  execute: async (name, args, revision, call_id) => {
    const output = await fetch(`${base}/api/tool`, { method: 'POST', headers, body: JSON.stringify({ name, args, revision, call_id }) }).then(r => r.json());
    console.log(JSON.stringify({ tool: name, isError: output.isError, target_id: args.target_id }));
    if (name === 'lab_highlight' && output.isError === false && JSON.parse(output.content[0].text).confirmed) confirmed = true;
    return output;
  },
});
const deadline = setTimeout(close, 45000);
const hardDeadline = setTimeout(() => socket.terminate(), 60000);
socket.on('open', () => send({ type: 'session.start', session: {
  model: process.env.OPENAI_LIVE_MODEL || 'gpt-live-1',
  instructions: liveInstructions,
  audio: { format: { type: 'audio/pcm', rate: 24000 }, output: { voice: 'marin' } },
  delegation: { type: 'responses', responses: {
    model: process.env.OPENAI_BACKEND_MODEL || 'gpt-6-sol',
    instructions: backendInstructions,
    tools: liveTools, tool_choice: 'auto', parallel_tool_calls: false,
  } },
} }));
socket.on('message', raw => {
  const e = JSON.parse(raw);
  eventTypes.add(e.type);
  if (e.type === 'session.started') {
    started = true; console.log(JSON.stringify({ event: e.type, model: e.session?.model }));
    let audioOffset = -24000;
    audioTimer = setInterval(() => {
      const chunk = Buffer.alloc(960);
      if (inputAudio && audioOffset >= 0 && audioOffset < inputAudio.length) inputAudio.copy(chunk, 0, audioOffset, audioOffset + 960);
      audioOffset += 960;
      if (!closing) send({ type: 'session.input_audio.append', audio: chunk.toString('base64') });
    }, 20);
    if (!inputAudio) {
      send({ type: 'response.item.create', item: { type: 'message', role: 'user', content: [{ type: 'input_text', text: `Прочитай состояние и подсвети указанный элемент ${process.env.TEST_TARGET_ID || config.state.allowed_targets[0]} независимо от текущего этапа, подпись: Проверка GPT-Live.` }] } });
      send({ type: 'response.create' });
    }
  } else if (e.type === 'session.output_audio.delta') {
    audioBytes += Buffer.from(e.delta, 'base64').length;
    if (confirmed && backendAnswered && audioBytes > 24000) close();
  } else if (e.type === 'error') {
    console.log(JSON.stringify({ event: 'error', code: e.error?.code ?? e.code,
      detail: JSON.stringify(e).replaceAll(process.env.OPENAI_API_KEY, '[redacted]').slice(0, 1500) })); close();
  } else if (e.type === 'session.input_transcript.delta' || e.type === 'session.output_transcript.delta') {
    console.log(JSON.stringify({ transcript: e.type, text: e.delta }));
  } else if (e.type === 'session.closed') { finalized = true; socket.close(); }
  if (e.type === 'response.event' && e.event?.type === 'response.output_item.done' && e.event.item?.type === 'message' && confirmed) backendAnswered = true;
  void loop.event(e).catch(() => { console.log('TOOL_LOOP_FAILED'); close(); });
});
socket.on('unexpected-response', (_req, response) => { console.log(JSON.stringify({ handshake_status: response.statusCode })); response.resume(); socket.terminate(); });
socket.on('error', () => console.log('LIVE_CONNECTION_ERROR'));
socket.on('close', () => {
  clearTimeout(deadline); clearTimeout(hardDeadline); clearInterval(audioTimer); loop.close();
  console.log(JSON.stringify({ started, highlight_confirmed: confirmed, output_audio_bytes: audioBytes, finalized, events: [...eventTypes] }));
  process.exitCode = started && confirmed && finalized && (!inputAudio || audioBytes > 0) ? 0 : 1;
});
