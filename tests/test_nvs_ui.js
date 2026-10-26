'use strict';
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const source=fs.readFileSync('web/interfaz_r4.html','utf8');
const api=source.slice(source.indexOf('async function api('),source.indexOf('async function sync()'));
const save=source.slice(source.indexOf('async function saveProgram(){'),source.indexOf('async function restoreProgram()'));
const fixture=JSON.parse(fs.readFileSync('tests/fixtures/v5_nvs_regression.json','utf8'));
const button={disabled:false},toasts=[];let requests=0,finish,timer;
const state={connected:true,profileGeneration:8,dirty:{pez:true,chi:true},pez:fixture.lights.pezverde.points,chi:fixture.lights.chihiros.points};
const ctx={state,AbortController,URLSearchParams,globalThis:null,console,JSON,Error,
 fetch:()=>{requests++;return new Promise(resolve=>{finish=resolve})},
 setTimeout:(fn,ms)=>{timer=ms;return 1},clearTimeout(){},
 isLive:()=>true,$:()=>button,confirm:()=>true,toast:t=>toasts.push(t),validCurve:()=>true,
 wireChecksum:()=>fixture.integrity.checksum,pointsWire:a=>a.map(p=>p.join(',')).join(';'),sync:async()=>{}};
ctx.globalThis=ctx;vm.createContext(ctx);vm.runInContext(api+save,ctx);
function respond(data,ok=false){finish({ok,status:ok?200:507,text:async()=>JSON.stringify(data)});}
(async()=>{
 let p=vm.runInContext('saveProgram()',ctx);assert(button.disabled);assert.equal(timer,15000);
 await vm.runInContext('saveProgram()',ctx);assert.equal(requests,1);
 respond({error:'No hay espacio NVS suficiente. Curva activa conservada.',nvs_error_code:4357,write_may_have_persisted:false});await p;
 assert.equal(toasts.at(-1),'No hay espacio NVS suficiente. Curva activa conservada.');assert(!toasts.at(-1).includes('{'));
 assert(state.dirty.pez&&state.dirty.chi&&!button.disabled&&!state.saving);assert.equal(state.profileGeneration,8);
 p=vm.runInContext('saveProgram()',ctx);respond({ok:true,generation:9,pv:38,chi:31},true);await p;
 assert.equal(state.profileGeneration,9);assert(!state.dirty.pez&&!state.dirty.chi&&!button.disabled);
 p=vm.runInContext('saveProgram()',ctx);respond({error:'Error commit',write_may_have_persisted:true});await p;
 assert(toasts.at(-1).includes('no confirmado'));
 console.log('PASS actual save UI: readable 507, dirty editor preserved, one request, confirmation only after success, uncertain-write warning');
})().catch(e=>{console.error(e);process.exitCode=1});
