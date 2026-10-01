import test from 'node:test';
import assert from 'node:assert/strict';
import { Lab } from '../server/lab.js';
import { laboratory } from '../server/laboratories.js';
import { instructionsFor } from '../server/scenario.js';
import { liveToolsFor } from '../server/mcp.js';
const snapshot = (lab, overrides={}) => ({ lab_id:'oberbeck',session_id:lab.id,scene_version:'ob-1',state_seq:1,stage:1,stages_total:2,stage_id:'first',finished:false,completed:[],allowed_targets:laboratory('oberbeck').targets.map(t=>t.id),instrument_state:{phase:'preparing',pulley:0,radius:.12,height:.6,current_height:.6,angle:0,stopwatch:0,timer_running:false,paused:false,records:[]},...overrides });
const body = r => JSON.parse(r.content[0].text);
test('Oberbeck catalog, state, completion and cross-lab rejection',async()=>{
 const l=new Lab();l.attach({send(){}},'unreal');l.update(snapshot(l));
 assert.equal(l.snapshot().lab_id,'oberbeck');
 assert.equal(body(await l.call('lab_highlight',{target_id:'panel.rotary',text:''})).error,'TARGET_OR_TEXT_NOT_ALLOWED');
 assert.throws(()=>l.update(snapshot(l,{lab_id:'unknown'})),/INVALID_LAB/);
 const before=l.revision;l.update(snapshot(l,{state_seq:2,instrument_state:{...snapshot(l).instrument_state,angle:1}}));assert.equal(l.revision,before);
 l.update(snapshot(l,{state_seq:3,stage:2,stage_id:'repeat',completed:['first']}));assert.ok(l.revision>before);
 l.update(snapshot(l,{state_seq:4,stage:2,stage_id:'complete',finished:true,completed:['first','repeat']}));assert.equal(l.snapshot().finished,true);
 assert.throws(()=>l.update(snapshot(l,{instrument_state:{...snapshot(l).instrument_state,radius:-1}})),/INVALID_SCENE/);
});
test('switching to menu invalidates pending command, tools and prompts are lab-specific',async()=>{
 const l=new Lab();l.attach({send(){}},'unreal');l.update(snapshot(l));
 const pending=l.call('lab_highlight',{target_id:'oberbeck.mass.1',text:'Груз'});
 l.update({lab_id:'menu',session_id:l.id,scene_version:'menu-2',state_seq:1,stage:1,stages_total:1,stage_id:'select',completed:[],allowed_targets:[],instrument_state:{}});
 assert.equal(body(await pending).error,'STATE_CHANGED');assert.equal(l.snapshot().connected,false);
 assert.match(instructionsFor('oberbeck').scenario.introduction,/Обербека/);
 assert.ok(!instructionsFor('oberbeck').backendInstructions.includes('773'));
 assert.ok(liveToolsFor('oberbeck').find(t=>t.name==='lab_highlight').parameters.properties.target_id.enum.every(id=>id.startsWith('oberbeck.')));
 assert.ok(liveToolsFor('training_panel').find(t=>t.name==='lab_highlight').parameters.properties.target_id.enum.every(id=>id.startsWith('panel.')));
});

const full = lab => ({lab_id:'oberbeck',mode:'full',session_id:lab.id,scene_version:'full-1',state_seq:1,stage:1,stages_total:5,stage_id:'unloaded',finished:false,completed:[],allowed_targets:laboratory('oberbeck','full').targets.map(t=>t.id),instrument_state:{phase:'preparing',pulley:0,mass_count:4,balanced:true,stopwatch:0,timer_running:false,paused:false,substep:'prepare_or_calculate',measurements:{D0:{value_si:.03,error_si:.0001}},series:[],checked_answers:[]}});
test('full mode separates progress, restricts public measurements and invalidates intro commands',async()=>{
 const l=new Lab();l.attach({send(){}},'unreal');l.update(snapshot(l));
 const pending=l.call('lab_highlight',{target_id:'oberbeck.mass.1',text:'Груз'});
 l.update(full(l));assert.equal(body(await pending).error,'STATE_CHANGED');assert.equal(l.snapshot().mode,'full');
 assert.equal(l.snapshot().stages_total,5);assert.ok(l.snapshot().allowed_targets.includes('oberbeck.caliper'));
 for(const hidden of ['radius','height','current_height','angle','reference_answers','true_j']){
   const state=full(l);state.state_seq=2;state.instrument_state[hidden]=.5;
   assert.throws(()=>l.update(state),/INVALID_SCENE/);
 }
 const state=full(l);state.state_seq=2;state.instrument_state.series=[{id:1,configuration:0,accepted:1,required:3,times:[2.35],true_h:.6}];assert.throws(()=>l.update(state),/INVALID_SCENE/);
 state.instrument_state.series[0]={id:1,configuration:0,accepted:1,required:3,times:[2.35]};l.update(state);
 assert.equal(l.snapshot().instrument_state.series[0].times[0],2.35);
 const before=l.revision;state.state_seq++;state.stage=2;state.stage_id='loaded';state.completed=['unloaded'];l.update(state);assert.ok(l.revision>before);
 const spec=instructionsFor('oberbeck','full');assert.match(spec.scenario.introduction,/инерцию/);assert.match(spec.backendInstructions,/по одному/);
 assert.ok(liveToolsFor('oberbeck','full').find(t=>t.name==='lab_highlight').parameters.properties.target_id.enum.includes('oberbeck.journal'));
 assert.ok(!liveToolsFor('oberbeck').find(t=>t.name==='lab_highlight').parameters.properties.target_id.enum.includes('oberbeck.journal'));
});

test('detailed guidance validates every nested object and never accepts hidden answers',async()=>{
 const l=new Lab();l.attach({send(){}},'unreal');const s=full(l),v=s.instrument_state;
 Object.assign(v,{masses:[1,2,3,4].map(id=>({id,installed:true})),view:0,calculation_task:1,selected_field:'measure:D0',active_series:0,attempt:{id:0,status:'none'},submitted_answers:{r0:{value_si:.015,unit:'m',correct:true}},feedback:{code:'reading_mismatch',message:'Проверьте отсчёт',action:'measure:D0',input:'3',unit:'mm'},next_action:{id:'remove_mass',text:'Снимите груз 1',target_id:'oberbeck.mass.1',field:''},visible_targets:['oberbeck.mass.1']});
 l.update(s);assert.equal(l.snapshot().instrument_state.feedback.code,'reading_mismatch');
 for(const mutate of [v=>v.masses[0].r=.2,v=>v.attempt.true_time=2.35,v=>v.submitted_answers.true_j={value_si:.01,unit:'kg*m2',correct:true},v=>v.next_action.answer=.01,v=>v.feedback.reference_answer=.01,v=>v.masses[0].id=2,v=>v.next_action.target_id='unknown',v=>delete v.feedback]){
   const bad=structuredClone(s);bad.state_seq++;mutate(bad.instrument_state);assert.throws(()=>l.update(bad),/INVALID_SCENE_STATE/);
 }
});

test('explicit clear cancels pending highlights and rejects old delegated calls',async()=>{
 const l=new Lab();const commands=[];const adapter={send:raw=>commands.push(JSON.parse(raw))};l.attach(adapter,'unreal');l.update(full(l));const revision=l.revision;
 const old=l.call('lab_highlight',{target_id:'oberbeck.mass.1',text:'Старое'},revision);
 const clear=l.call('lab_clear_highlight',{},revision);
 assert.equal(body(await old).error,'CANCELLED');
 assert.equal(body(await l.call('lab_highlight',{target_id:'oberbeck.mass.2',text:'Позднее'},revision)).error,'STALE_CONTEXT');
 l.ack(adapter,{...commands[0],ok:true});l.ack(adapter,{...commands[1],ok:true});assert.equal(body(await clear).confirmed,true);
});
