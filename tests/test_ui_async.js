'use strict';
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const html=fs.readFileSync('web/interfaz_r4.html','utf8'),source=html.slice(html.indexOf('async function api('),html.indexOf('function hourLabel('));
const f=JSON.parse(fs.readFileSync('tests/fixtures/v5_nvs_regression.json','utf8')).lights;
const pending=[],renders=[];
const state={connected:true,updated:0,dirty:{chi:true,pez:true},chi:[[1,1,1,1]],pez:[[1,1,1,1,0]],profileGeneration:8};
const ctx={state,isLive:()=>true,AbortController,URLSearchParams,Date,Number,JSON,Set,Error,
 setTimeout:()=>1,clearTimeout(){},setCadenceLabel(){},validCurve:()=>true,
 render(){renders.push(state.status?.profileGeneration)},fetch:(path,opt)=>new Promise((resolve,reject)=>pending.push({path,opt,resolve,reject}))};
vm.createContext(ctx);vm.runInContext(source,ctx);
const call=s=>vm.runInContext(s,ctx),tick=()=>new Promise(r=>setImmediate(r));
function reply(p,data,ok=true){p.resolve({ok,status:ok?200:500,text:async()=>JSON.stringify(data)})}
function status(gen){return {rgb:[0,0,0],ble:true,profileGeneration:gen,points:f.pezverde.points.map(p=>p.join(',')).join(';'),chiPoints:f.chihiros.points.map(p=>p.join(',')).join(';')}}
(async()=>{
 const a=call('sync()'),b=call('sync()');assert.equal(pending.length,1);
 const write=call("api('/api/clock',{epoch:1})");assert.equal(pending.length,2);
 await assert.rejects(call("api('/api/clock',{epoch:1})"),/en curso/);assert.equal(pending.length,2);
 reply(pending[1],{ok:true});await write;reply(pending[0],status(9));await tick();assert.equal(pending.length,3);assert.equal(pending[2].path,'/api/status');
 reply(pending[2],status(10));await Promise.all([a,b]);assert.equal(state.status.profileGeneration,10);assert(!renders.includes(9));
 assert.equal(state.profileGeneration,8);assert.deepEqual(state.chi,[[1,1,1,1]]);assert.equal(state.liveCurves.chi.length,31);
 const failure=call("api('/api/clock',{epoch:2})");reply(pending.at(-1),{error:'Fallo legible'},false);await assert.rejects(failure,/Fallo legible/);
 const retry=call("api('/api/clock',{epoch:2})");reply(pending.at(-1),{ok:true});await retry;
 const lost=call('sync()');pending.at(-1).reject(Error('network'));await lost;assert.equal(state.connected,false);
 console.log('PASS API concurrency: repeated writes rejected, one in-flight status GET, stale read discarded after a write, dirty editor preserved, live curves independent and failed lock released');
})().catch(e=>{console.error(e);process.exitCode=1});
