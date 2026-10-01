import { readFile, writeFile } from 'node:fs/promises';
import os from 'node:os';
const [input,output='.runtime/voice-timing.json']=process.argv.slice(2);
if(!input)throw new Error('Usage: node scripts/report-voice-timing.js events.jsonl [report.json]');
const events=(await readFile(input,'utf8')).split('\n').filter(Boolean).map(line=>JSON.parse(line));
function summarize(values,failed=0){values.sort((a,b)=>a-b);return {n:values.length,failed,median_ms:values.length?values[Math.floor(values.length/2)]:null,p95_ms:values.length?values[Math.ceil(values.length*.95)-1]:null};}
const report={created:new Date().toISOString(),host:{platform:os.platform(),arch:os.arch(),release:os.release(),cpu:os.cpus()[0]?.model},method:{input:'20ms PCM frames; RMS > 0.015 for 60ms; speech end after 300ms below threshold',output:'PCM consumed by Unreal renderer; RMS > sqrt(0.00001); silence 200ms; interruption timeout 3s',scope:'Software estimates. Excludes DAC/headphone latency. Noise/echo can affect speech activity; manually validate candidate interruptions. Use headphones.',target_samples_per_voice_metric:20},results:{}};
for(const kind of ['speech_end_to_highlight_ack','interruption_renderer']){const rows=events.filter(e=>e.type==='voice_metric'&&e.kind===kind);report.results[kind]=summarize(rows.filter(e=>e.success).map(e=>e.milliseconds),rows.filter(e=>!e.success).length);}
const tools=events.filter(e=>e.type==='tool_result'&&e.name==='lab_highlight').map(e=>{try{return JSON.parse(e.output.content[0].text);}catch{return {};}});
report.results.mcp_send_to_unreal_ack=summarize(tools.filter(e=>e.confirmed).map(e=>e.latency_ms),tools.filter(e=>!e.confirmed).length);
report.voice_sample_count_sufficient=['speech_end_to_highlight_ack','interruption_renderer'].every(k=>report.results[k].n>=20);
report.manual_audio_acceptance='required';
await writeFile(output,JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify({report:output,results:report.results,voice_sample_count_sufficient:report.voice_sample_count_sufficient}));
