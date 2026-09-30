import express from 'express';
import { createServer } from 'node:http';
import { fileURLToPath } from 'node:url';
import { resolve } from 'node:path';
import { mkdir, writeFile } from 'node:fs/promises';
import { WebSocketServer } from 'ws';
import { voiceGateway } from './voice.js';
import { Lab, result } from './lab.js';
import { handleMcp, callMcp, liveTools } from './mcp.js';

export async function createApp({ port = 3210, apiKey = process.env.OPENAI_API_KEY, connectProvider, onDesktopExit } = {}) {
  const app = express();
  const http = createServer(app);
  const sockets = new WebSocketServer({ noServer: true, maxPayload: 65536 });
  const lab = new Lab();
  let baseUrl;
  const liveCalls = new Map();
  const voice = voiceGateway({ lab, apiKey, baseUrl: () => baseUrl, connectProvider, onDesktopExit });
  app.use((req, res, next) => {
    if (!['127.0.0.1', 'localhost'].includes(req.hostname)) return res.status(403).json({ error: 'LOCAL_HOST_ONLY' });
    const origin = req.headers.origin;
    if (origin && ![`http://localhost:${http.address()?.port}`, baseUrl].includes(origin)) return res.status(403).json({ error: 'ORIGIN_NOT_ALLOWED' });
    res.set('Cache-Control', 'no-store');
    next();
  });
  app.use(express.json({ limit: '96kb' }));
  app.get('/api/bootstrap', (_req, res) => res.json({
    session_id: lab.id, token: lab.token, state: lab.snapshot(),
    key_configured: Boolean(apiKey), model: process.env.OPENAI_LIVE_MODEL || 'gpt-live-1',
  }));
  const authorize = (req, res, next) => {
    if (req.headers.authorization !== `Bearer ${lab.token}` || req.headers['x-lab-session'] !== lab.id)
      return res.status(401).json({ error: 'INVALID_LAB_SESSION' });
    next();
  };
  app.get('/api/state', authorize, (_req, res) => res.json({ state: lab.snapshot(), events: lab.events }));
  app.post('/mcp', authorize, (req, res, next) => handleMcp(req, res, lab).catch(next));
  app.get('/mcp', authorize, (_req, res) => res.status(405).end());
  app.delete('/mcp', authorize, (_req, res) => res.status(405).end());
  app.post('/api/tool', authorize, async (req, res) => {
    const { name, args = {}, revision, call_id } = req.body;
    if (!liveTools.some(tool => tool.name === name) || !Number.isInteger(revision) ||
        (call_id !== undefined && (typeof call_id !== 'string' || call_id.length > 200)))
      return res.status(400).json({ error: 'INVALID_TOOL_REQUEST' });
    // Deduplicate forwarded Live calls, including retries while still in flight.
    let pending = call_id && liveCalls.get(call_id);
    if (!pending) {
      pending = callMcp(baseUrl, lab, name, args, revision).catch(() => result({ error: 'MCP_TRANSPORT_FAILED' }, true));
      if (call_id) {
        liveCalls.set(call_id, pending);
        if (liveCalls.size > 1000) liveCalls.delete(liveCalls.keys().next().value);
      }
    }
    res.json(await pending);
  });
  app.use((err, _req, res, _next) => res.status(err.status || 500).json({ error: 'REQUEST_FAILED' }));
  http.on('upgrade', (req, socket, head) => {
    const origin = req.headers.origin;
    // Unreal/libwebsockets sends the loopback host as Origin without the server port.
    // Pairing still requires the per-session secret in hello.
    const sceneOrigins = [baseUrl, `http://localhost:${http.address().port}`, 'http://127.0.0.1', '127.0.0.1'];
    if (!['/scene', '/voice'].includes(req.url) || !['localhost', '127.0.0.1'].includes((req.headers.host || '').split(':')[0]) ||
        (origin && !sceneOrigins.includes(origin))) return socket.destroy();
    const target = req.url === '/voice' ? voice.sockets : sockets;
    target.handleUpgrade(req, socket, head, ws => target.emit('connection', ws));
  });
  sockets.on('connection', ws => {
    let authenticated = false;
    const timer = setTimeout(() => ws.close(1008, 'HELLO_TIMEOUT'), 5000);
    ws.on('message', raw => {
      try {
        const message = JSON.parse(raw.toString());
        if (!authenticated) {
          if (message.type !== 'hello' || message.session_id !== lab.id || message.token !== lab.token ||
              message.adapter !== 'unreal') throw new Error('INVALID_HELLO');
          lab.attach(ws, message.adapter);
          authenticated = true;
          clearTimeout(timer);
          ws.send(JSON.stringify({ type: 'hello_ack', session_id: lab.id }));
        } else if (message.type === 'state') lab.update(message.state);
        else if (message.type === 'ack') lab.ack(ws, message);
        else throw new Error('UNKNOWN_MESSAGE');
      } catch { ws.close(1008, 'INVALID_MESSAGE_OR_SCENE_BUSY'); }
    });
    ws.on('error', () => {});
    ws.on('close', () => { clearTimeout(timer); lab.detach(ws); });
  });
  await new Promise(resolve => http.listen(port, '127.0.0.1', resolve));
  baseUrl = `http://127.0.0.1:${http.address().port}`;
  return { app, http, lab, baseUrl, close: async () => {
    await voice.close();
    for (const ws of sockets.clients) ws.terminate();
    sockets.close();
    http.closeAllConnections();
    await new Promise(resolve => http.close(resolve));
  } };
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const runtime = await createApp({ port: Number(process.env.PORT || 3210) });
  const runtimeDir = process.env.LAB_RUNTIME_DIR || '.runtime';
  await mkdir(runtimeDir, { recursive: true, mode: 0o700 });
  await writeFile(resolve(runtimeDir, 'connection.json'), JSON.stringify({ url: runtime.baseUrl.replace('http:', 'ws:') + '/scene',
    session_id: runtime.lab.id, token: runtime.lab.token }), { mode: 0o600 });
  runtime.lab.on('event', event => console.log(JSON.stringify(event)));
  console.log(`Five cubes: ${runtime.baseUrl}. API key configured: ${Boolean(process.env.OPENAI_API_KEY)}`);
  if (process.env.LAB_PARENT_PID) setInterval(() => {
    try { process.kill(Number(process.env.LAB_PARENT_PID), 0); } catch { void runtime.close().finally(() => process.exit(0)); }
  }, 2000).unref();
  for (const signal of ['SIGINT', 'SIGTERM']) process.once(signal, async () => { await runtime.close(); process.exit(0); });
}
