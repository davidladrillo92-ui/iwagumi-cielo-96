'use strict';
// Tests de navegador aislados: sin servidor ni credenciales de red.
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const html=fs.readFileSync('web/interfaz_r4.html','utf8');
const js=html.split('<script>',2)[1].split('</script>',1)[0];
const start=js.indexOf('function isLive()');
const end=js.indexOf('// Local access',start);
assert(start>=0&&end>start);
const f=js.slice(start,end);
for(const [protocol,hostname,search,want] of [
 ['https:','iwagumi-cielo96.local','',true],
 ['https:','192.168.0.25','',true],
 ['http:','192.168.0.25','',true],
 ['https:','localhost','',false],
 ['file:','','',false],
]){
 const ctx={location:{protocol,hostname,search}};vm.createContext(ctx);vm.runInContext(f,ctx);
 assert.strictEqual(vm.runInContext('isLive()',ctx),want,`${protocol}//${hostname}`);
}
// Commands go directly to the local API; no authentication or retry request.
const a=js.indexOf('async function api('),b=js.indexOf('async function sync()',a);
const calls=[];const ctx={isLive:()=>true,URLSearchParams,
 fetch:async(path,opt)=>{calls.push([path,opt]);return {ok:true,text:async()=>'{"ok":true}'}}};
vm.createContext(ctx);vm.runInContext(js.slice(a,b),ctx);
(async()=>{
 await vm.runInContext("api('/api/cielo/v4/pause',{confirm:'PAUSAR_TODO'})",ctx);
 assert.equal(calls.length,1);assert.equal(calls[0][0],'/api/cielo/v4/pause');
 assert.equal(calls[0][1].method,'POST');
 assert.equal(calls[0][1].body.toString(),'confirm=PAUSAR_TODO');
 assert.deepEqual(Object.keys(calls[0][1].headers),['Content-Type']);
 console.log('PASS local HTTP: live UI and single direct command without login');
})().catch(err=>{console.error(err);process.exitCode=1});
