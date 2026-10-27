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
 assert.equal(elements.chiSub.textContent,'Conectado');assert.equal(elements.pezSub.textContent,'Conectado');
 assert.equal(elements.chiInfo.textContent,'0 / 0 / 0');assert.equal(elements.diagState.textContent,'Todo en orden');
 assert.equal(elements.diagPvStatus.textContent,'Conectado');assert.equal(elements.diagChStatus.textContent,'Conectado');
 assert(!requests.some(p=>p.startsWith('/api/lab')||p.startsWith('/api/auth')));
 vm.runInContext('settings()',ctx);assert.equal(typeof elements.chNightOn.onclick,'function');
 assert.equal(typeof elements.chNightOff.onclick,'function');assert.equal(typeof elements.cieloPauseAll.onclick,'function');
 vm.runInContext('state.status.cieloV4.ch_reconnect_pending=true;render()',ctx);
 assert.equal(elements.chiSub.textContent,'Reconectando…');
 // Never trust a stale recent flag if the link snapshot says disconnected.
 vm.runInContext('state.status.cieloV4.pv_link_connected=false;render()',ctx);
 assert.equal(elements.pezSub.textContent,'Sin enlace BLE');assert.equal(elements.pezPct.textContent,'—');
 vm.runInContext('state.status.cieloV4.pv_link_idle_released=true;render()',ctx);
 assert.equal(elements.pezSub.textContent,'Conectado');
 vm.runInContext('state.status.cieloV4.pv_link_idle_released=false;state.status.cieloV4.pv_link_connected=true;state.status.cieloV4.pv_online_recent=false;render()',ctx);
 assert.equal(elements.pezSub.textContent,'Sin verificar');
 // Report89: OFF acknowledged126s ago, next check300s, not 'connected' nor unknown.
 vm.runInContext("Object.assign(state.status.cieloV4,{pv_waiting_off_check:true,pv_off_obligation:true,pv_safety_state:'APAGADO_GATT_OK'});render()",ctx);
 assert.equal(elements.pezSub.textContent,'Conectado · en reposo');assert.equal(elements.pezPct.textContent,'0%');
 assert.equal(elements.pezLed.textContent,'☾');assert.equal(elements.pezLed.className,'conn-tick rest');assert(elements.pezLed.title.includes('Último apagado'));assert(!elements.pezLed.title.includes('GATT reciente'));
 sample.lighting.natural_v4.pv_waiting_off_check=true;sample.lighting.natural_v4.pv_link_connected=true;
 sample.lighting.natural_v4.pv_off_obligation=true;sample.lighting.natural_v4.pv_safety_state='APAGADO_GATT_OK';
 ctx.sample=sample;vm.runInContext('diagRender(sample)',ctx);assert.equal(elements.diagPvStatus.textContent,'Conectado · en reposo');
 vm.runInContext('state.status.cieloV4.pv_link_connected=false;render()',ctx);
 assert.equal(elements.pezSub.textContent,'Sin enlace BLE');
 vm.runInContext('state.status.cieloV4.pv_waiting_off_check=false;render()',ctx);
 sample.lighting.natural_v4.pv_waiting_off_check=false;
 vm.runInContext('state.status.cieloV4.pv_waiting_off_check=true;state.status.cieloV4.pv_link_connected=true;state.status.cieloV4.pv_off_failures=1;render()',ctx);
 assert.equal(elements.pezSub.textContent,'Reconectando…');assert.notEqual(elements.pezLed.className,'conn-tick rest');
 vm.runInContext('state.status.cieloV4.pv_off_failures=0;state.connected=false;render()',ctx);
 assert.equal(elements.pezSub.textContent,'Sin comunicación');assert.notEqual(elements.pezLed.textContent,'☾');
 vm.runInContext('state.connected=true;state.preview=0;render()',ctx);assert.equal(elements.pezSub.textContent,'Simulación');
 vm.runInContext('state.preview=null;state.status.cieloV4.pv_waiting_off_check=false;render()',ctx);

 // Regression 00:20: POWER OFF GATT wins over stale RGB 58/78/132 at 6%.
 vm.runInContext("Object.assign(state.status.cieloV4,{pv_link_idle_released:true,pv_link_connected:false,pv_online_recent:true,pv_last_requested:[58,78,132,6],pv_off_obligation:true,pv_safety_state:'APAGADO_GATT_OK'});render()",ctx);
 assert.equal(elements.pezPct.textContent,'0%');
 assert.equal(elements.pezInfo.textContent,'0 / 0 / 0');
 assert.equal(elements.pezSub.textContent,'Conectado');
 assert(elements.pezPct.title.includes('GATT'));
 vm.runInContext("state.status.cieloV4.pv_safety_state='APAGADO_PENDIENTE';render()",ctx);
 assert.equal(elements.pezPct.textContent,'—');
 assert.equal(elements.pezInfo.textContent,'Apagado pendiente');
 vm.runInContext("state.status.cieloV4.pv_off_obligation=false;state.status.cieloV4.pv_safety_state='ACTIVA';render()",ctx);
 assert.equal(elements.pezPct.textContent,'6%');
 assert.equal(vm.runInContext("scene(1410)[0]",ctx),'Luz de luna');
 assert.equal(vm.runInContext("scene(20)[0]",ctx),'Descanso nocturno');
 vm.runInContext("state.status.clockValid=false;render()",ctx);
 assert(elements.statusText.textContent.includes('hora pendiente'));
 assert(elements.momentSub.textContent.includes('Hora del móvil'));
 vm.runInContext("state.status.clockValid=true;render()",ctx);

 vm.runInContext('state.connected=false;render()',ctx);assert.equal(elements.chiSub.textContent,'Sin comunicación');
 assert.equal(elements.chiPct.textContent,'—');
 vm.runInContext('state.connected=true;state.updated=Date.now()-21000;render()',ctx);
 assert.equal(elements.chiSub.textContent,'Sin comunicación');assert.equal(elements.chiPct.textContent,'—');
 // Recovered historical failures must not be displayed as an active alarm.
 sample.status='avisos_historicos';sample.alerts={current_critical:false,historical_soft_timeouts:4};
 ctx.sample=sample;vm.runInContext('diagRender(sample)',ctx);assert.equal(elements.diagState.textContent,'Todo en orden');
 sample.lighting.natural_v4.pv_online_recent=false;vm.runInContext('diagRender(sample)',ctx);
 assert.equal(elements.diagPvStatus.textContent,'Sin comunicación reciente');
 sample.bluetooth.worker_unresponsive=true;vm.runInContext('diagRender(sample)',ctx);assert.equal(elements.diagState.textContent,'Revisar conexión');
 for(const id of ['authBadge','accessAction','cadenceTrial','cadenceNormal','chLivePanel','lastConfirmedPv','lastConfirmedChi','diagView','diagShare','diagCopy','labGuided'])assert(!ids.includes(id),id);
 assert(ids.includes('loadProfile')&&ids.includes('saveCurve')&&ids.includes('exportProfile'));
 assert.equal(ids.filter(id=>['diagDownload','diagCopy','diagView','diagShare'].includes(id)).length,1);
 console.log('PASS SIMPLE1 strict DOM: init, compact home/status, no login/lab calls, settings, stale/retrying/historical states');
})().catch(e=>{console.error(e);process.exitCode=1});
