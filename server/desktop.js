import { readFile, mkdir, writeFile, appendFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { createApp } from './index.js';
const options = Object.fromEntries(process.argv.slice(2).map((v, i, a) => v.startsWith('--') ? [v.slice(2), a[i + 1]] : []).filter(v => v.length));
const directory = options.runtime;
if (!directory || !options.settings || !options.parent) throw new Error('Desktop launch arguments missing');
try {
  const settings = JSON.parse(await readFile(options.settings, 'utf8'));
  if (typeof settings.env_file === 'string') process.loadEnvFile(settings.env_file);
} catch { console.error('KEY_CONFIGURATION_UNAVAILABLE'); }
const runtime = await createApp({ port: 0, onDesktopExit: () => stop() });
await mkdir(directory, { recursive: true, mode: 0o700 });
await writeFile(resolve(directory, 'connection.json'), JSON.stringify({ url: runtime.baseUrl.replace('http:', 'ws:') + '/scene', session_id: runtime.lab.id, token: runtime.lab.token }), { mode: 0o600 });
let stopping = false;
async function stop() { if (stopping) return; stopping = true; await runtime.close(); process.exit(0); }
for (const signal of ['SIGINT', 'SIGTERM']) process.once(signal, stop);
setInterval(() => { try { process.kill(Number(options.parent), 0); } catch { void stop(); } }, 1000).unref();
runtime.lab.on('event', event => {
  void appendFile(resolve(directory, 'events.jsonl'), JSON.stringify(event) + '\n', { mode: 0o600 }).catch(() => {});
});
console.log('DESKTOP_SERVER_READY');
