import { z } from 'zod';
const str = z.string().max(500);
const field = z.string().max(80);
const finite = z.number().finite();
const answerKey = z.string().regex(/^(r[01]|[tj][0-3]|d[tj][0-3]|mean[01]|mass_j|point_j|a[23]|alpha[23]|tension[23]|M[23]|ratio[23]|error_mean[01]|rel_mean[01]|approx_(rel|abs)[02])$/);
const record = z.object({ value_si: finite, unit: z.string().max(24), correct: z.boolean() }).strict();
export const guidanceKeys = ['masses','view','calculation_task','selected_field','active_series','attempt','submitted_answers','feedback','next_action','visible_targets'];
const schema = z.object({
  masses: z.array(z.object({id:z.number().int().min(1).max(4),installed:z.boolean()}).strict()).length(4).refine(a=>new Set(a.map(m=>m.id)).size===4),
  view: z.number().int().min(0).max(3), calculation_task:z.number().int().min(1).max(5), selected_field:field,
  active_series:z.number().int().nonnegative(),
  attempt:z.object({id:z.number().int().nonnegative(),status:z.enum(['none','pending','accepted','excluded','early','interrupted'])}).strict(),
  submitted_answers:z.record(answerKey,record),
  feedback:z.object({code:z.enum(['','invalid_number_or_unit','instrument_alignment','reading_mismatch','configuration','missing_measurement','incomplete_series','measurement_stale','height_stop','missing_series','pending_attempt','configuration_changed','invalid_attempt','series_complete','insufficient_data','invalid_action','calculation','conclusion','coefficients']),message:str,action:field,input:z.string().max(120),unit:z.string().max(24)}).strict(),
  next_action:z.object({id:field,text:str,target_id:field,field}).strict(),
  visible_targets:z.array(field),
}).strict();
export function validGuidance(v, allowed) {
  // Older snapshots remain valid; partial new contracts are rejected.
  if(!guidanceKeys.some(k=>k in v)) return true;
  const selected=Object.fromEntries(guidanceKeys.map(k=>[k,v[k]]));
  if(!schema.safeParse(selected).success)return false;
  return v.masses.filter(m=>m.installed).length===v.mass_count &&
    (!v.next_action.target_id || allowed.includes(v.next_action.target_id)) &&
    v.visible_targets.every(id=>allowed.includes(id));
}
// Significant context excludes continuous stopwatch telemetry and viewport clipping.
export function contextSignature(state) {
  if(!state)return '';
  const {stopwatch,visible_targets,...rest}=state.instrument_state;
  return JSON.stringify({scene:state.scene_version,stage:state.stage_id,lab:state.lab_id,mode:state.mode,instruments:rest});
}
