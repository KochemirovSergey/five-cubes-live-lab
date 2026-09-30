import { scenario } from '../server/scenario.js';
import { TARGETS } from '../server/targets.js';
export const stateFor = (lab, index = 0, overrides = {}) => ({
  session_id: lab.id, scene_version: 'scene-a', state_seq: (lab.state?.state_seq ?? 0) + 1,
  stage: Math.min(index + 1, 4), stages_total: 4,
  stage_id: index === 4 ? 'complete' : scenario.steps[index].id, finished: index === 4,
  completed: scenario.steps.slice(0, index).map(s => s.id),
  allowed_targets: [...TARGETS],
  instrument_state: { rotary_angle: 0, slider: 'left', toggle_up: false, keypad_input: '', code_error: false },
  ...overrides,
});
