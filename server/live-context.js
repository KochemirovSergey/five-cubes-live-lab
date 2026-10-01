// Live append content is limited to 500 tokens. A <=480 UTF-8 byte payload
// stays below that limit even in the worst case of one token per byte.
// Full measurements, answers and target catalogs remain available through MCP.
export function liveContext(state) {
  const i = state.instrument_state || {};
  const summary = {
    source: 'lab_get_state', lab: state.lab_id, mode: state.mode,
    stage: state.stage, finished: Boolean(state.finished),
    phase: i.phase, next: i.next_action?.id, view: i.view,
    field: i.selected_field, error: i.feedback?.code,
    timer_running: i.timer_running, paused: i.paused,
  };
  for (const key of ['field', 'next', 'error', 'phase', 'mode', 'lab']) {
    if (Buffer.byteLength(JSON.stringify(summary), 'utf8') <= 480) break;
    delete summary[key];
  }
  const content = JSON.stringify(summary);
  return Buffer.byteLength(content, 'utf8') <= 480 ? content : '{"source":"lab_get_state","changed":true}';
}
