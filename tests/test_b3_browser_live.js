'use strict';
// Tests de navegador aislados: sin servidor ni credenciales de red.
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const html=fs.readFileSync('web/interfaz_r4.html','utf8');
const js=html.split('<script>',2)[1].split('</script>',1)[0];
const start=js.indexOf('function isLive()');
const end=js.indexOf('// El token solo vive',start);
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
// Una sesión compartida para solicitudes simultaneas, sin token persistente.
const s=js.indexOf("let cieloSessionToken=''");
const e=js.indexOf('async function api(',s);
assert(s>=0&&e>s);
const auth=js.slice(s,e);
let fetchCount=0;
const token='a'.repeat(64);
const context={
 $:()=>({value:'clave-privada-para-test'}),
 fetch:async(url,opt)=>{fetchCount++;assert.strictEqual(url,'/api/auth/session');
  assert.strictEqual(opt.headers['X-Cielo-Key'],'clave-privada-para-test');
  return {ok:true,json:async()=>({session:token})};},
};
vm.createContext(context);vm.runInContext(auth,context);
(async()=>{
 const results=await Promise.all(Array.from({length:4},()=>vm.runInContext('cieloEnsureSession()',context)));
 assert.deepStrictEqual(results,[token,token,token,token]);
 assert.strictEqual(fetchCount,1);
 assert.strictEqual(vm.runInContext('cieloSessionToken',context),token);
 console.log('OK B3 Safari HTTPS/IP, consulta en vivo y sesion unica concurrente');
})().catch(err=>{console.error(err);process.exitCode=1});
