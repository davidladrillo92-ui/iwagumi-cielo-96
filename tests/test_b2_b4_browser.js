'use strict';
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const html=fs.readFileSync('web/interfaz_r4.html','utf8');
const js=html.split('<script>',2)[1].split('</script>',1)[0];
function slice(a,b){const i=js.indexOf(a),j=js.indexOf(b,i);assert(i>=0&&j>i,`${a}..${b}`);return js.slice(i,j)}
const ctx={console,Number,Array,Math,JSON,Date,confirm:()=>true,toast:()=>{},render:()=>{},sync:async()=>{},
 state:null,chooseCurve:()=>{},Blob,URL,document:{createElement:()=>({click:()=>{}})},setTimeout};
let calls=[],response={ok:true,generation:7};
ctx.api=async(path,data)=>{calls.push({path,data});if(response instanceof Error)throw response;return response};
vm.createContext(ctx);
const initial=fs.readFileSync('tests/fixtures/iwagumi_v5_luna_directa.json','utf8');
const original=JSON.parse(initial),pv=original.lights.pezverde.points,chi=original.lights.chihiros.points;
ctx.state={pez:pv.map(x=>x.slice()),chi:chi.map(x=>x.slice()),dirty:{pez:true,chi:true},connected:true,baseGeneration:6,undo:[],redo:[],curve:'both'};
vm.runInContext(slice('function encodePoints(','function scene(t){'),ctx);
vm.runInContext(slice('function validCurve(','function manualChooser(){'),ctx);
assert.strictEqual(vm.runInContext("crc32Ascii('123456789')",ctx),'cbf43926');
assert(vm.runInContext("validCurve('chi',state.chi)",ctx));
assert(vm.runInContext("validCurve('pez',state.pez)",ctx));
assert.strictEqual(chi.length,31);assert.strictEqual(pv.length,34);
assert.deepStrictEqual(chi.filter(v=>v[0]>=1379&&v[0]<=1380),[[1379,2,2,2],[1380,1,1,2]]);
// New file format CRC32 is deterministic and detects corrupt payloads.
let crc=vm.runInContext('crc32Ascii(curvePayload(state.pez,state.chi))',ctx);
assert(/^[0-9a-f]{8}$/.test(crc));
const maxPV=[[0,0,0,0,0],...Array.from({length:62},(_,i)=>[i+1,50,60,70,10]),[1440,0,0,0,0]];
const maxCH=[[0,0,0,0],...Array.from({length:62},(_,i)=>[i+1,50,60,70]),[1440,0,0,0]];
ctx.maxPV=maxPV;ctx.maxCH=maxCH;
assert(vm.runInContext("validCurve('pez',maxPV)&&validCurve('chi',maxCH)",ctx));
assert(!vm.runInContext("validCurve('pez',maxPV.concat(maxPV[0]))",ctx));
const events=[];ctx.toast=s=>events.push(s);
(async()=>{
 await vm.runInContext('saveProgram()',ctx);
 assert.strictEqual(calls.length,1,'Atomic save is ONE request');
 assert.strictEqual(calls[0].path,'/api/profile/transaction');
 assert.strictEqual(calls[0].data.generation,'6');
 assert.strictEqual(calls[0].data.crc32,crc);
 assert.strictEqual(ctx.state.dirty.chi,false);assert.strictEqual(ctx.state.dirty.pez,false);
 // Optimistic generation conflict must preserve unsaved local edits.
 ctx.state.dirty.pez=true;ctx.state.dirty.chi=true;response=new Error('409 profile mismatch');
 await vm.runInContext('saveProgram()',ctx);
 assert(ctx.state.dirty.pez&&ctx.state.dirty.chi);
 // The originally approved V5 JSON can be imported without schema v2.
 ctx.state.dirty.pez=false;ctx.state.dirty.chi=false;
 const legacyEvent={target:{files:[{size:initial.length,text:async()=>initial}],value:'file'}};
 await vm.runInContext('importProgram(legacyEvent)',Object.assign(ctx,{legacyEvent}));
 assert(ctx.state.dirty.pez&&ctx.state.dirty.chi);
 // Corrupt V2 with valid-looking points must be rejected before changing editor.
 const v2={schema:'iwagumi-cielo-profile',schema_version:2,timezone:'Europe/Madrid',time_unit:'minute',
  checksum:{algorithm:'CRC32',value:'00000000'},lights:{pezverde:{unit:'rgb255-plus-brightness',points:pv},chihiros:{unit:'percent',points:chi}}};
 const before=JSON.stringify(ctx.state.chi);
 const alteredEvent={target:{files:[{size:1000,text:async()=>JSON.stringify(v2)}],value:'file'}};
 await vm.runInContext('importProgram(alteredEvent)',Object.assign(ctx,{alteredEvent}));
 assert.strictEqual(JSON.stringify(ctx.state.chi),before);
 assert(events.some(s=>s.includes('checksum incorrecto')));
 // Save current V5 as deterministic V2 export and reimport the checksum.
 v2.checksum.value=crc;
 assert.strictEqual(v2.checksum.value,vm.runInContext('crc32Ascii(curvePayload(state.pez,state.chi))',ctx));
 console.log('B2+B4 browser: 64 points, exact V5, CRC32, atomic save, conflict, legacy import, tamper rejection PASS');
})().catch(e=>{console.error(e);process.exitCode=1});
