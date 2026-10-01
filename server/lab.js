import { randomUUID, randomBytes } from 'node:crypto';
import { EventEmitter } from 'node:events';

import { laboratory, oberbeckProfile as physics } from './laboratories.js';
import { guidanceKeys, validGuidance } from './oberbeck-state.js';
import { TARGETS } from './targets.js';
export { TARGETS };
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
      connected: Boolean(this.adapter && this.state && this.state.lab_id !== 'menu'), adapter: this.adapter?.kind ?? null };
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
    const labId = state.lab_id ?? 'training_panel';
    const mode = state.mode ?? 'intro';
    if (labId === 'oberbeck' && !['intro', 'full'].includes(mode)) throw new Error('INVALID_MODE');
    const spec = laboratory(labId, mode);
    if (!spec) throw new Error('INVALID_LAB');
    const scenario = spec.scenario;
    const count = scenario.steps.length;
    const index = state.stage - 1;
    const finished = state.finished === true;
    const step = scenario.steps[index];
    const expectedCompleted = scenario.steps.slice(0, finished ? count : index).map(s => s.id);
    const allowed = spec.targets.map(t => t.id);
    const instruments = state.instrument_state;
    if (state.session_id !== this.id || typeof state.scene_version !== 'string' || !state.scene_version ||
        !Number.isInteger(state.state_seq) || state.state_seq < 1 || !step ||
        state.stages_total !== count || state.stage_id !== (finished ? 'complete' : step.id) ||
        (finished && state.stage !== count) || JSON.stringify(state.completed) !== JSON.stringify(expectedCompleted) ||
        JSON.stringify(state.allowed_targets) !== JSON.stringify(allowed) || !instruments ||
        !validInstruments(labId, instruments, mode)) throw new Error('INVALID_SCENE_STATE');
    if (this.state?.scene_version === state.scene_version && state.state_seq <= this.state.state_seq) return;
    const changed = !this.state || this.state.scene_version !== state.scene_version || this.state.stage_id !== state.stage_id || this.state.lab_id !== labId || this.state.mode !== mode;
    if (changed) { this.invalidate('STATE_CHANGED'); this.revision++; }
    this.state = { lab_id: labId, mode, session_id: this.id, scene_version: state.scene_version, state_seq: state.state_seq,
      stage: state.stage, stages_total: count, stage_id: state.stage_id, finished,
      stage_goal: finished ? 'Упражнение выполнено' : step.goal,
      completed: [...state.completed], allowed_targets: allowed,
      instrument_state: structuredClone(instruments) };
    this.log(changed ? 'stage_changed' : 'state', { state: this.snapshot() });
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
    if (name === 'lab_clear_highlight') this.cancel();
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

function validInstruments(id, v, mode) {
  if (id === 'oberbeck' && mode === 'full') return validFullInstruments(v);
  if (id === 'menu') return true;
  if (id === 'training_panel') return Number.isFinite(v.rotary_angle) && ['left','right'].includes(v.slider) && typeof v.toggle_up === 'boolean' && typeof v.keypad_input === 'string' && /^\d{0,3}$/.test(v.keypad_input) && typeof v.code_error === 'boolean';
  return ['preparing','falling','landed'].includes(v.phase) && [0,1].includes(v.pulley) &&
    ['radius','height','current_height','angle','stopwatch'].every(k => Number.isFinite(v[k])) &&
    v.radius >= physics.r_min && v.radius <= physics.r_max && v.height >= 0 && v.height <= physics.h_max && v.current_height >= 0 && v.current_height <= v.height + 1e-9 && v.stopwatch >= 0 &&
    typeof v.timer_running === 'boolean' && typeof v.paused === 'boolean' && Array.isArray(v.records) && v.records.length <= 2 &&
    v.records.every(r => [r.radius,r.height,r.time].every(Number.isFinite) && r.radius >= physics.r_min && r.radius <= physics.r_max && r.height >= physics.h_min && r.height <= physics.h_max && r.time >= 0 && [0,1].includes(r.pulley));
}

function validFullInstruments(v) {
  const keys = ['phase','pulley','mass_count','balanced','stopwatch','timer_running','paused','substep','measurements','series','checked_answers',...guidanceKeys];
  // Fail closed: accidental private model fields must never reach the voice context.
  if (Object.keys(v).some(k => !keys.includes(k)) || !validGuidance(v,laboratory('oberbeck','full').targets.map(t=>t.id))) return false;
  return ['preparing','falling','landed','replay'].includes(v.phase) && [0,1].includes(v.pulley) &&
    Number.isInteger(v.mass_count) && v.mass_count >= 0 && v.mass_count <= 4 &&
    ['balanced','timer_running','paused'].every(k => typeof v[k] === 'boolean') && Number.isFinite(v.stopwatch) && v.stopwatch >= 0 &&
    ['review','wait_for_landing','stop_timer_after_landing','copy_time_to_journal','prepare_or_calculate'].includes(v.substep) &&
    v.measurements && !Array.isArray(v.measurements) && Object.entries(v.measurements).every(([k,m]) =>
      ['D0','D1','R','h'].includes(k) && Object.keys(m).every(key => ['value_si','error_si'].includes(key)) && Number.isFinite(m.value_si) && m.value_si > 0 && Number.isFinite(m.error_si) && m.error_si > 0) &&
    Array.isArray(v.series) && v.series.length <= 4 && new Set(v.series.map(s => s.configuration)).size === v.series.length &&
    v.series.every(s => Object.keys(s).every(k => ['id','configuration','accepted','required','times'].includes(k)) &&
      Number.isInteger(s.id) && s.id > 0 && [0,1,2,3].includes(s.configuration) && [3,4,5].includes(s.required) &&
      Number.isInteger(s.accepted) && s.accepted >= 0 && s.accepted <= s.required && Array.isArray(s.times) && s.times.length === s.accepted && s.times.every(t => Number.isFinite(t) && t > 0)) &&
    Array.isArray(v.checked_answers) && v.checked_answers.every(k => typeof k === 'string' && /^[a-zA-Z0-9_]{1,30}$/.test(k));
}
