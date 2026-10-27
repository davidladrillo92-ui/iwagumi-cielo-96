'use strict';
// Strict DOM IDs: init must not reference controls removed from the supplied screenshots.
const fs=require('fs'),vm=require('vm'),assert=require('assert');
const html=fs.readFileSync('web/interfaz_r4.html','utf8'),js=html.split('<script>')[1].split('</script>')[0];
const ids=[...html.matchAll(/\bid="([^"]+)"/g)].map(m=>m[1]);
assert.equal(new Set(ids).size,ids.length);
const elements={};
function element(id){return {id,value:'',textContent:'',disabled:false,dataset:{},style:{setProperty(){}},
 classList:{toggle(){},add(){},remove(){},contains(){return false}},setAttribute(){},focus(){},getClientRects(){return [1]},
 getBoundingClientRect(){return {left:0,width:338}},addEventListener(){},
 set innerHTML(s){this._html=s;for(const m of s.matchAll(/\bid="([^"]+)"/g))elements[m[1]]=element(m[1])},
 get innerHTML(){return this._html||''}}}
for(const id of ids)elements[id]=element(id);
const sample={uptime_sec:723,lighting:{natural_v4:{pv_online_recent:true,ch_online_recent:true,pv_link_idle_released:true,ch_link_idle_released:true}},professional:{pez_verde:'CORRECTO',chihiros:'CORRECTO'},
 network:{home_wifi_connected:true},bluetooth:{per_device:{pez_verde:{gatt_ok:2},chihiros:{gatt_ok:2}}}};
const status={rgb:[0,0,0],ble:true,clockValid:true,cieloV4:{always_on:true,chihiros_active:true,pez_verde_active:true,
 ch_online_recent:true,pv_online_recent:true,ch_last_requested:[0,0,0],pv_last_requested:[0,0,0,0]},firmware:'MOCK'};
const requests=[];
const ctx={console,Date,Math,Number,JSON,Blob,URLSearchParams,
 location:{protocol:'http:',hostname:'192.168.0.25',search:''},
 window:{addEventListener(){}},document:{body:{style:{}},getElementById:id=>elements[id]||null,querySelectorAll:()=>[],addEventListener(){}},
 setTimeout(){},clearTimeout(){},setInterval(){},confirm:()=>true,
 fetch:async path=>{requests.push(path);return {ok:true,json:async()=>path==='/api/status'?status:sample,
 text:async()=>JSON.stringify(path==='/api/status'?status:path==='/api/service/status'?{otaEnabled:true,version:'MOCK'}:sample)}}};
vm.createContext(ctx);vm.runInContext(js,ctx);
(async()=>{
 vm.runInContext('init()',ctx);await new Promise(r=>setImmediate(r));await new Promise(r=>setImmediate(r));
 function equalStatus(){assert.equal(elements.chiSub.textContent,elements.diagChStatus.textContent);assert.equal(elements.pezSub.textContent,elements.diagPvStatus.textContent);}
 equalStatus();assert.equal(elements.chiSub.textContent,'Conectado · en reposo');assert.equal(elements.pezSub.textContent,elements.chiSub.textContent);
 assert.equal(elements.chiLed.textContent,'☾');assert.equal(elements.pezLed.textContent,'☾');assert.equal(elements.diagState.textContent,'Todo en orden');
 assert(!requests.some(p=>p.startsWith('/api/diagnostico')||p.startsWith('/api/lab')||p.startsWith('/api/auth')));
 vm.runInContext('settings()',ctx);assert.equal(typeof elements.chNightOn.onclick,'function');
 assert.equal(typeof elements.chNightOff.onclick,'function');assert.equal(typeof elements.cieloPauseAll.onclick,'function');
 vm.runInContext('state.status.cieloV4.ch_reconnect_pending=true;render()',ctx);equalStatus();assert.equal(elements.chiSub.textContent,'Reconectando…');
 vm.runInContext('state.status.cieloV4.ch_reconnect_pending=false;state.status.cieloV4.pv_link_connected=false;render()',ctx);
 equalStatus();assert.equal(elements.pezSub.textContent,'Sin enlace BLE');assert.equal(elements.pezPct.textContent,'—');
 vm.runInContext('state.status.cieloV4.pv_link_idle_released=true;render()',ctx);assert.equal(elements.pezSub.textContent,'Conectado · en reposo');
 vm.runInContext('state.status.cieloV4.pv_link_idle_released=false;state.status.cieloV4.pv_link_connected=true;state.status.cieloV4.pv_online_recent=false;render()',ctx);
 equalStatus();assert.equal(elements.pezSub.textContent,'Comprobando conexión');
 vm.runInContext("Object.assign(state.status.cieloV4,{pv_waiting_off_check:true,pv_off_obligation:true,pv_safety_state:'APAGADO_GATT_OK'});render()",ctx);
 equalStatus();assert.equal(elements.pezSub.textContent,'Conectado · en reposo');assert.equal(elements.pezPct.textContent,'0%');
 assert.equal(elements.pezLed.className,'conn-tick rest');assert(elements.pezLed.title.includes('no verificada'));
 vm.runInContext('state.status.cieloV4.pv_off_failures=1;render()',ctx);equalStatus();assert.equal(elements.pezSub.textContent,'Reconectando…');
 vm.runInContext('state.status.cieloV4.pv_off_failures=0;state.connected=false;render()',ctx);equalStatus();assert.equal(elements.pezSub.textContent,'Sin comunicación');
 vm.runInContext('state.connected=true;state.updated=Date.now()-21000;refreshLiveDisplay()',ctx);equalStatus();assert.equal(elements.diagState.textContent,'Sin comunicación con el ATOM');
 vm.runInContext('state.updated=Date.now();state.view="curves";state.preview=900;render()',ctx);equalStatus();assert.equal(elements.pezSub.textContent,'Conectado · en reposo');
 vm.runInContext('nav("light")',ctx);assert.equal(vm.runInContext('state.preview',ctx),null);
 vm.runInContext("Object.assign(state.status.cieloV4,{pv_link_idle_released:true,pv_link_connected:false,pv_online_recent:true,pv_last_requested:[58,78,132,6],pv_off_obligation:true,pv_safety_state:'APAGADO_GATT_OK'});render()",ctx);
 assert.equal(elements.pezPct.textContent,'0%');assert.equal(elements.pezInfo.textContent,'0 / 0 / 0');equalStatus();
 vm.runInContext("state.status.cieloV4.pv_safety_state='APAGADO_PENDIENTE';render()",ctx);assert.equal(elements.pezPct.textContent,'—');assert.equal(elements.pezInfo.textContent,'Apagado pendiente');
 vm.runInContext("state.status.cieloV4.pv_off_obligation=false;state.status.cieloV4.pv_safety_state='ACTIVA';render()",ctx);assert.equal(elements.pezPct.textContent,'6%');equalStatus();
 assert.equal(vm.runInContext('scene(1410)[0]',ctx),'Luz de fondo');assert.equal(vm.runInContext('scene(20)[0]',ctx),'En reposo');
 vm.runInContext('state.status.clockValid=false;state.status.health={current_critical:true};render()',ctx);assert(elements.statusText.textContent.includes('hora pendiente'));assert.equal(elements.diagState.textContent,'Esperando la hora');
 vm.runInContext('state.status.clockValid=true;state.status.health={current_critical:true};render()',ctx);assert.equal(elements.diagState.textContent,'Revisar CIELO');
 vm.runInContext('state.status.health={current_critical:false};render()',ctx);assert.equal(elements.diagState.textContent,'Todo en orden');
 for(const id of ['authBadge','accessAction','cadenceTrial','cadenceNormal','chLivePanel','lastConfirmedPv','lastConfirmedChi','diagView','diagShare','diagCopy','labGuided'])assert(!ids.includes(id),id);
 assert(ids.includes('loadProfile')&&ids.includes('saveCurve')&&ids.includes('exportProfile'));
 console.log('PASS unified strict DOM: both lamps and both views identical, zero/day, failures, stale state, preview isolation, OFF priority, lightweight polling and health');
})().catch(e=>{console.error(e);process.exitCode=1});
