import { test } from 'node:test';
import assert from 'node:assert/strict';
import { liveContext } from '../server/live-context.js';

test('Live context remains under append token limit as full lab history grows', () => {
  const state = {lab_id:'oberbeck',mode:'full',stage:5,finished:true,instrument_state:{
    phase:'landed',view:2,selected_field:'mass_j',next_action:{id:'complete'},feedback:{code:''},
    submitted_answers:Object.fromEntries(Array.from({length:1000},(_,i)=>[i,{value_si:i,correct:true}])),
    series:Array(1000).fill({text:'Длинная история измерений'}),
  }};
  const content=liveContext(state);
  assert.ok(Buffer.byteLength(content)<=480);
  assert.deepEqual(JSON.parse(content),{source:'lab_get_state',lab:'oberbeck',mode:'full',stage:5,finished:true,phase:'landed',next:'complete',view:2,field:'mass_j',error:''});
  assert.ok(!content.includes('value_si'));
  state.instrument_state.selected_field='Д'.repeat(1000);
  assert.ok(Buffer.byteLength(liveContext(state))<=480);
});


test('full lab reference reaches the backend separately from bounded Live updates', async () => {
  const { instructionsFor } = await import('../server/scenario.js');
  const spec = instructionsFor('oberbeck','full');
  assert.match(spec.backendInstructions, /Теоретическое введение/);
  assert.match(spec.backendInstructions, /Контрольные вопросы/);
  assert.match(spec.backendInstructions, /с учетом сил трения/);
  for(let i=1;i<=5;i++) assert.ok(spec.backendInstructions.includes(`Задание ${i}.`));
  assert.match(spec.backendInstructions, /Не называй численный ответ/);
  assert.ok(!spec.backendInstructions.includes('https://'));
});
