import { randomUUID, randomBytes } from 'node:crypto';
import { EventEmitter } from 'node:events';

export const TARGETS = ['cube_1', 'cube_2', 'cube_3', 'cube_4', 'cube_5'];
export const result = (data, isError = false) => ({
  isError, content: [{ type: 'text', text: JSON.stringify(data) }],
});

export class Lab extends EventEmitter {
  constructor({ timeoutMs = 4000 } = {}) {
    super();
    this.id = randomUUID();
    this.token = randomBytes(32).toString('hex');
    this.timeoutMs = timeoutMs;
    this.adapter = null;
    this.pending = new Map();
    this.revision = 0;
    this.state = null;
    this.events = [];
  }
  log(type, details = {}) {
    const event = { time: new Date().toISOString(), type, ...details };
    this.events.push(event);
    if (this.events.length > 100) this.events.shift();
    this.emit('event', event);
  }
  snapshot() {
    return { ...this.state, session_id: this.id, revision: this.revision,
      connected: Boolean(this.adapter && this.state), adapter: this.adapter?.kind ?? null };
  }
  attach(socket, kind) {
    if (this.adapter) throw new Error('SCENE_ALREADY_CONNECTED');
    this.adapter = { socket, kind };
    this.state = null;
    this.revision++;
    this.log('scene_connected', { adapter: kind });
  }
  detach(socket) {
    if (this.adapter?.socket !== socket) return;
    this.adapter = null;
    this.state = null;
    this.revision++;
    this.invalidate('SCENE_DISCONNECTED');
    this.log('scene_disconnected');
  }
  invalidate(reason) {
    for (const pending of this.pending.values()) pending.finish(result({ error: reason }, true));
  }
  cancel() {
    this.revision++;
    this.invalidate('CANCELLED');
    this.log('cancelled');
  }
  update(state) {
    if (state.session_id !== this.id || typeof state.scene_version !== 'string' ||
        !state.scene_version || state.stage_id !== 'five_cubes' || state.stage !== 1 ||
        state.stages_total !== 1 || !Array.isArray(state.allowed_targets) ||
        state.allowed_targets.some(id => !TARGETS.includes(id)) ||
        !Array.isArray(state.completed)) throw new Error('INVALID_SCENE_STATE');
    this.invalidate('STATE_CHANGED');
    this.revision++;
    this.state = { session_id: this.id, scene_version: state.scene_version, stage: 1,
      stages_total: 1, stage_id: 'five_cubes', stage_goal: 'Попросите подсветить один из пяти кубиков',
      completed: state.completed, allowed_targets: [...state.allowed_targets], instrument_state: {} };
    this.log('state', { state: this.snapshot() });
  }
  ack(socket, message) {
    if (this.adapter?.socket !== socket || message.session_id !== this.id) return;
    const pending = this.pending.get(message.command_id);
    if (!pending || message.scene_version !== pending.scene_version) return;
    pending.finish(result(message.ok === true
      ? { confirmed: true, command_id: message.command_id, target_id: pending.target_id,
          adapter: this.adapter.kind, latency_ms: Date.now() - pending.started }
      : { error: typeof message.error === 'string' ? message.error.slice(0, 200) : 'SCENE_REJECTED' }, message.ok !== true));
  }
  async call(name, args = {}, expectedRevision = this.revision) {
    if (!this.adapter || !this.state) return result({ error: 'SCENE_NOT_READY' }, true);
    if (expectedRevision !== this.revision) return result({ error: 'STALE_CONTEXT' }, true);
    if (name === 'lab_get_state') return result(this.snapshot());
    if (!['lab_highlight', 'lab_clear_highlight'].includes(name)) return result({ error: 'UNKNOWN_TOOL' }, true);
    if (name === 'lab_highlight' && (!this.state.allowed_targets.includes(args.target_id) ||
        typeof args.text !== 'string' || args.text.length > 120)) return result({ error: 'TARGET_OR_TEXT_NOT_ALLOWED' }, true);
    const command_id = randomUUID();
    const command = { type: name === 'lab_highlight' ? 'highlight' : 'clear',
      command_id, session_id: this.id, scene_version: this.state.scene_version,
      stage_id: this.state.stage_id, target_id: args.target_id, text: args.text };
    return new Promise(resolve => {
      const started = Date.now();
      const timer = setTimeout(() => finish(result({ error: 'ACK_TIMEOUT', command_id }, true)), this.timeoutMs);
      const finish = output => {
        if (!this.pending.has(command_id)) return;
        clearTimeout(timer);
        this.pending.delete(command_id);
        this.log('tool_result', { name, command_id, output });
        resolve(output);
      };
      this.pending.set(command_id, { finish, started, scene_version: command.scene_version, target_id: args.target_id });
      this.log('tool_call', { name, command_id, target_id: args.target_id });
      try { this.adapter.socket.send(JSON.stringify(command)); }
      catch { finish(result({ error: 'SEND_FAILED' }, true)); }
    });
  }
}
