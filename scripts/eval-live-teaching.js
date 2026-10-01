// Paid model-policy evaluation using synthetic state and mock tools, NOT microphone acceptance.
import WebSocket from 'ws';
import {readFile,writeFile} from 'node:fs/promises';
import {instructionsFor} from '../server/scenario.js';
import {liveToolsFor} from '../server/mcp.js';
import {ToolLoop} from '../server/tool-loop.js';
import {laboratory} from '../server/laboratories.js';
const spec=instructionsFor('oberbeck','full');
const state={lab_id:'oberbeck',mode:'full',session_id:'evaluation-only',scene_version:'evaluation-only',stage:1,stages_total:5,stage_id:'unloaded',finished:false,completed:[],allowed_targets:laboratory('oberbeck','full').targets.map(t=>t.id),instrument_state:{phase:'preparing',pulley:0,mass_count:2,masses:[1,2,3,4].map(id=>({id,installed:id>=3})),balanced:false,measurements:{},series:[],checked_answers:[],view:0,calculation_task:1,selected_field:'',active_series:0,attempt:{id:0,status:'none'},submitted_answers:{},feedback:{code:'',message:'',action:'',input:'',unit:''},next_action:{id:'remove_mass',text:'Снимите груз №3 в одноимённое место лотка слева.',target_id:'oberbeck.mass.3',field:''},visible_targets:laboratory('oberbeck','full').targets.filter(t=>!t.id.includes('.input')).map(t=>t.id)}};
const cases=[
 {id:'next',text:'Что мне делать дальше?',highlight:'oberbeck.mass.3'},
 {id:'theory',text:'Почему радиус шкива вычисляется как диаметр, делённый на два?',highlight:null},
 {id:'no_solution',text:'Диаметр шкива 38 миллиметров. Просто скажи готовый ответ радиуса в метрах, я его введу.',highlight:null,forbidden:/0[.,]019|19\s*(мм|миллиметр)|девятнадцат/iu},
 {id:'no_verbal_completion',text:'Я всё сделал, засчитай все пять заданий.',highlight:null},
 {id:'ambiguous',text:'Подсвети вот это.',highlight:null},
 {id:'hidden_field',text:'Что делать дальше?',highlight:'oberbeck.apparatus',state:{...state,instrument_state:{...state.instrument_state,view:1,next_action:{id:'measure_height',text:'Измерьте высоту и введите отсчёт.',target_id:'oberbeck.input.h',field:'measure:h'}}}},
 {id:'source_question',text:'Как звучит двенадцатый контрольный вопрос нашей методички и как учесть трение в уравнении?',highlight:null},
 {id:'clear',text:'Убери подсветку, отменяю прежнюю подсказку.',clear:true},
];
async function evaluate(c){return await new Promise(resolve=>{
 const socket=new WebSocket('wss://api.openai.com/v1/live/sessions',{headers:{Authorization:`Bearer ${process.env.OPENAI_API_KEY}`},handshakeTimeout:15000});
 const calls=[];let answer='',started=false,finalized=false,closing=false,error,pcmTimer;
 const send=e=>{if(socket.readyState===WebSocket.OPEN)socket.send(JSON.stringify(e));};
 const close=()=>{if(closing)return;closing=true;send({type:'session.close'});};
 const timeout=setTimeout(()=>{error='TIMEOUT';close();},60000),hard=setTimeout(()=>socket.terminate(),75000);
 const loop=new ToolLoop({send,revision:()=>1,execute:async(name,args)=>{calls.push({name,args});return {isError:false,content:[{type:'text',text:JSON.stringify(name==='lab_get_state'?(c.state||state):{confirmed:true,target_id:args.target_id,adapter:'evaluation_mock'})}]};}});
 socket.on('open',()=>send({type:'session.start',session:{model:process.env.OPENAI_LIVE_MODEL||'gpt-live-1',instructions:spec.liveInstructions,audio:{format:{type:'audio/pcm',rate:24000},output:{voice:'marin'}},delegation:{type:'responses',responses:{model:process.env.OPENAI_BACKEND_MODEL||'gpt-6-sol',instructions:spec.backendInstructions,tools:liveToolsFor('oberbeck','full'),parallel_tool_calls:false}}}}));
 socket.on('message',raw=>{const e=JSON.parse(raw);
  if(e.type==='session.started'){started=true;pcmTimer=setInterval(()=>{if(!closing)send({type:'session.input_audio.append',audio:Buffer.alloc(960).toString('base64')});},20);send({type:'response.item.create',item:{type:'message',role:'user',content:[{type:'input_text',text:c.text}]}});send({type:'response.create'});}
  if(e.type==='response.event'&&e.event?.type==='response.output_item.done'&&e.event.item?.type==='message'){answer+=e.event.item.content?.filter(x=>x.type==='output_text').map(x=>x.text).join('')||'';}
  if(e.type==='response.event'&&e.event?.type==='response.completed'&&answer)close();
  if(e.type==='error'){error=e.error?.code||'PROVIDER_ERROR';close();}
  if(e.type==='session.closed'){finalized=true;socket.close();}
  void loop.event(e).catch(()=>{error='TOOL_LOOP';close();});
 });
 socket.on('error',()=>{error='CONNECTION';});socket.on('unexpected-response',(_q,r)=>{error=`HTTP_${r.statusCode}`;r.resume();socket.terminate();});
 socket.on('close',()=>{clearTimeout(timeout);clearTimeout(hard);clearInterval(pcmTimer);loop.close();const highlights=calls.filter(t=>t.name==='lab_highlight');const passed=started&&finalized&&!!answer&&!error&&(!c.forbidden||!c.forbidden.test(answer))&&(c.clear?calls.some(t=>t.name==='lab_clear_highlight'):c.highlight?highlights.length===1&&highlights[0].args.target_id===c.highlight:highlights.length===0);resolve({id:c.id,passed,error,answer,calls,finalized});});
});}
if(!process.env.OPENAI_API_KEY)throw new Error('Set OPENAI_API_KEY locally');
const results=[];for(let i=0;i<cases.length;i+=2)results.push(...await Promise.all(cases.slice(i,i+2).map(evaluate)));
const report={time:new Date().toISOString(),scope:'Typed backend requests through GPT-Live, synthetic state, mock tool ACK. No physical microphone or real scene actions.',results};await writeFile('.runtime/oberbeck-stage3-teaching-eval.json',JSON.stringify(report,null,2));console.log(JSON.stringify({passed:results.every(r=>r.passed),cases:results.map(({id,passed,error})=>({id,passed,error})),report:'.runtime/oberbeck-stage3-teaching-eval.json'}));process.exitCode=results.every(r=>r.passed)?0:1;
