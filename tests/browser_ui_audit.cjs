'use strict';
// Real browser, simulated HTTP only. Never point this runner at physical hardware.
const fs=require('fs'),assert=require('assert');
const {chromium}=require(require.resolve('playwright',{paths:[process.env.CIELO_BROWSER_MODULES||process.env.CODEX_PRIMARY_RUNTIME_NODE_MODULES||process.cwd()]}));
(async()=>{
 const browser=await chromium.launch({headless:true,...(process.env.CIELO_CHROMIUM?{executablePath:process.env.CIELO_CHROMIUM,args:['--no-sandbox','--disable-gpu','--disable-dev-shm-usage']}: {})});
 const page=await browser.newPage({viewport:{width:390,height:844}}),errors=[],requests=[],buttons=new Set();
 const html=fs.readFileSync('web/interfaz_r4.html','utf8'),fixture=JSON.parse(fs.readFileSync('tests/fixtures/v5_nvs_regression.json','utf8'));
 let generation=10,boot=140,canUpdate=true,failSave=false,phase='idle',scanning=false,failWifi=false;
 const points=structuredClone(fixture.lights);
 const v={always_on:true,chihiros_active:true,pez_verde_active:true,ch_link_connected:true,pv_link_connected:true,ch_online_recent:true,pv_online_recent:false,pv_waiting_off_check:true,pv_off_obligation:true,pv_safety_state:'APAGADO_GATT_OK',ch_last_requested:[0,0,0],pv_last_requested:[7,11,16,1]};
 const work=()=>({active:!['idle','checking'].includes(phase),preparing:phase==='checking',phase,can_start:phase==='idle',remaining_sec:phase==='rising'?600:phase==='returning'?120:0});
 const status=()=>({rgb:[0,0,0],ble:true,clockValid:true,clock:'27/10/2026 12:19:00',firmware:'MOCK3.11',boot_id:boot,health:{boot_ready:true,current_critical:false},cieloV4:v,work_light:work(),profileGeneration:generation,points:points.pezverde.points.map(p=>p.join(',')).join(';'),chiPoints:points.chihiros.points.map(p=>p.join(',')).join(';')});
 page.on('pageerror',e=>errors.push(e.message));page.on('dialog',d=>d.accept());
 await page.route('http://cielo.test/**',async route=>{
  const req=route.request(),u=new URL(req.url()),path=u.pathname;requests.push({path,method:req.method()});
  if(path==='/')return route.fulfill({contentType:'text/html',body:html});
  let data={ok:true};let code=200;
  if(path==='/api/status')data=status();
  else if(path==='/api/service/status')data={otaEnabled:true,version:'MOCK3.11'};
  else if(path==='/api/diagnostico/maestro')return route.fulfill({contentType:'application/json',body:JSON.stringify({schema:'mock-report'})});
  else if(path==='/api/work-light/start'){phase='checking';data=work();}
  else if(path==='/api/work-light/stop'){phase=phase==='checking'?'idle':'returning';data=work();}
  else if(path==='/api/curves/commit'){
   if(failSave){code=507;data={error:'Guardado rechazado: perfil anterior conservado'};}
   else{const p=new URLSearchParams(req.postData());points.pezverde.points=p.get('pv').split(';').map(x=>x.split(',').map(Number));points.chihiros.points=p.get('ch').split(';').map(x=>x.split(',').map(Number));data={ok:true,generation:++generation,pv:points.pezverde.points.length,chi:points.chihiros.points.length};}
  }else if(path==='/api/monitor/status')data={enabled:true,last_poll_age_sec:0};
  else if(path==='/api/maintenance')data={flash_backup:'USB_REQUIRED_NOT_PERFORMED'};
  else if(path==='/api/wifi/scan'){scanning=true;await new Promise(r=>setTimeout(r,50));}
  else if(path==='/api/wifi/networks')data={pending:false,networks:[{ssid:'Casa <script>error()</script>',rssi:-40}]};
  else if(path==='/api/wifi'&&failWifi){code=507;data={ok:false,error:'No se pudo guardar la Wi-Fi completa. La conexion actual se mantiene.'};}
  else if(path==='/api/ota/preflight')data={ready:canUpdate,reason:canUpdate?'':'Arranque pendiente'};
  else if(path==='/api/ota/prepare/status')data={phase:'ready'};
  else if(path==='/api/ota/upload'){assert.match(req.headers()['x-cielo-sha256'],/^[0-9a-f]{64}$/);boot++;}
  return route.fulfill({status:code,contentType:'application/json',body:JSON.stringify(data)});
 });
 async function auditButtons(){
  const snapshot=await page.evaluate(()=>Array.from(document.querySelectorAll('button')).filter(e=>e.getClientRects().length).map(e=>{const key=['view','curve','lamp','zoom'].find(k=>e.dataset[k]!==undefined);return {id:e.id||(key?key+':'+e.dataset[key]:''),label:e.textContent.trim(),bound:typeof e.onclick==='function'}}));
  for(const info of snapshot){assert(info.bound,JSON.stringify(info));buttons.add(info.id||info.label);}
 }
 async function sync(){await page.evaluate(()=>sync());}
 async function settings(){await page.locator('#openSettings').click();await auditButtons();}
 async function more(){await page.locator('#modalBody summary').click();await auditButtons();}
 await page.goto('http://cielo.test/');await page.waitForFunction(()=>document.getElementById('chiSub').textContent==='Conectado · en reposo');await auditButtons();
 for(const width of [320,390,768]){
  await page.setViewportSize({width,height:844});
  assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
  assert.equal(await page.locator('#chiSub').textContent(),await page.locator('#pezSub').textContent());
  assert.equal(await page.locator('#chiLed').textContent(),await page.locator('#pezLed').textContent());
  if(process.env.CIELO_SHOTS){fs.mkdirSync(process.env.CIELO_SHOTS,{recursive:true});await page.screenshot({path:process.env.CIELO_SHOTS+'/luz-'+width+'.png',fullPage:true});}
  await page.locator('[data-view="lab"]').click();await auditButtons();
  assert.equal(await page.locator('#diagChStatus').textContent(),await page.locator('#chiSub').textContent());
  assert.equal(await page.locator('#diagPvStatus').textContent(),await page.locator('#pezSub').textContent());
  if(process.env.CIELO_SHOTS)await page.screenshot({path:process.env.CIELO_SHOTS+'/centro-'+width+'.png',fullPage:true});
  await page.locator('[data-view="light"]').click();
 }
 await page.setViewportSize({width:390,height:844});
 await page.locator('#workLightStart').click();await page.waitForFunction(()=>document.getElementById('workLightState').textContent==='Comprobando luces…');
 await page.locator('#workLightStop').click();assert.equal(phase,'idle');
 await page.locator('#workLightStart').click();phase='rising';await sync();assert(await page.locator('#workLightProgress').isVisible());
 phase='holding';await sync();assert(!await page.locator('#workLightProgress').isVisible());await page.locator('#workLightStop').click();assert.equal(phase,'returning');phase='idle';await sync();
 await page.locator('[data-view="curves"]').click();await auditButtons();
 await page.waitForFunction(()=>getComputedStyle(document.getElementById('toast')).opacity==='0');
 const previewPosts=requests.filter(r=>r.method==='POST').length;
 for(const width of [320,390,768]){
  await page.setViewportSize({width,height:844});await page.locator('#scrub').fill('901');
  assert(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
  assert.equal(await page.locator('#scrubTime').textContent(),'15:01');
  assert.equal(await page.locator('#scrub').getAttribute('aria-valuetext'),'15:01');
  assert.equal(await page.locator('.curve-preview [data-lamp]').count(),2);
  assert.equal(await page.locator('#chartInspector').count(),0);
  assert.equal(await page.locator('#viewCurves').getByText('HORA SIMULADA',{exact:false}).count(),0);
  assert(await page.evaluate(()=>{
   const ch=vAt(state.chi,901),pv=vAt(state.pez,901);
   return $('curveChiPct').textContent===Math.max(...ch)+'%'&&$('curvePezPct').textContent===pv[3]+'%'&&
    $('curveChiRgb').textContent==='R '+ch[0]+' · G '+ch[1]+' · B '+ch[2]&&
    $('curvePezRgb').textContent==='R '+pv[0]+' · G '+pv[1]+' · B '+pv[2];
  }));
  const chart=await page.locator('#chart').boundingBox(),axis=await page.locator('#chartAxisTimes').boundingBox(),preview=await page.locator('.curve-preview').boundingBox();
  assert(axis.y>=chart.y+chart.height-1&&preview.y>axis.y);
  if(process.env.CIELO_SHOTS){
   await page.screenshot({path:process.env.CIELO_SHOTS+'/curvas-'+width+'.png',fullPage:true});
   if(width===390){await page.locator('.curve-workspace').evaluate(e=>e.scrollIntoView({block:'start'}));await page.locator('.curve-workspace').screenshot({path:process.env.CIELO_SHOTS+'/detalle-curvas-390.png'});}
  }
 }
 await page.setViewportSize({width:390,height:844});
 for(const t of [0,1380,1440]){
  await page.locator('#scrub').fill(String(t));
  assert(await page.evaluate(t=>$('scrubTime').textContent===hourLabel(t)&&$('curvePezPct').textContent===vAt(state.pez,t)[3]+'%',t));
 }
 await page.locator('[data-curve="chi"]').click();await page.locator('[data-curve="pez"]').click();await page.locator('[data-curve="both"]').click();
 await page.locator('#chart circle[data-which="chi"][data-index="10"]').click();await auditButtons();
 assert(await page.locator('#editSelectedPoint').isVisible());await page.locator('#editSelectedPoint').click();await page.locator('#pointR').waitFor();await page.locator('#sheetClose').click();
 await page.locator('.curve-preview [data-lamp="pez"]').click();await page.locator('#pointR').waitFor();await page.locator('#sheetClose').click();
 assert.equal(requests.filter(r=>r.method==='POST').length,previewPosts); // Time, graph, rows and editor opening never transmit.
 await page.locator('[data-curve="both"]').click();
 await page.locator('[data-zoom="1"]').click();await page.locator('[data-zoom="3"]').click();await page.locator('[data-zoom="6"]').click();await page.locator('[data-zoom="24"]').click();await page.locator('#compareCurves').click();
 await page.locator('#scrub').fill('1100');await page.locator('#resetTime').click();
 await page.locator('#editCurve').click();await auditButtons();await page.locator('#pointBack').click();await page.locator('#pointForward').click();await page.locator('#pointR').fill('2');await page.locator('#pointApply').click();await page.locator('#pointAdd').click();await auditButtons();await page.locator('#confirmNewPoint').click();await page.locator('#pointRemove').click();await page.locator('#sheetClose').click();
 await page.locator('#undoCurve').click();await page.locator('#redoCurve').click();
 await page.locator('#importFile').setInputFiles({name:'perfil.json',mimeType:'application/json',buffer:Buffer.from(JSON.stringify(fixture))});
 failSave=true;await page.locator('#saveCurve').click();await page.waitForFunction(()=>!state.saving);assert(await page.locator('#saveCurve').isEnabled());failSave=false;
 await page.locator('#saveCurve').click();await page.waitForFunction(()=>!state.saving);assert(!await page.locator('#saveCurve').isEnabled());
 const download=page.waitForEvent('download');await page.locator('#exportProfile').click();assert.equal((await download).suggestedFilename(),'IWAGUMI-CIELO-perfil-v2.json');
 await page.locator('#curvesAdvanced summary').click();await page.locator('#resetCurve').click();await page.locator('#undoCurve').click();
 await settings();await page.locator('#wifiHome').click();await auditButtons();await page.locator('#wifiScan').click();await page.locator('#sheetClose').click();await page.waitForTimeout(900);assert(scanning);
 await settings();await page.locator('#wifiHome').click();await page.locator('#wifiScan').click();await page.waitForFunction(()=>document.getElementById('wifiNetworks').options.length===2);
 assert.equal(await page.locator('#wifiNetworks option').last().textContent(),'Casa <script>error()</script> · -40 dBm');
 await page.locator('#wifiSsid').fill('Casa');await page.locator('#wifiPass').fill('test-password');failWifi=true;await page.locator('#wifiTry').click();await page.waitForFunction(()=>!api.pending.has('/api/wifi'));assert(await page.locator('#wifiTry').isVisible());assert.equal(await page.locator('#wifiSsid').inputValue(),'Casa');failWifi=false;await page.locator('#wifiTry').click();await page.waitForFunction(()=>!document.getElementById('modal').classList.contains('open'));
 await settings();await page.locator('#wifiHome').click();await page.locator('#wifiForget').click();await page.waitForFunction(()=>!document.getElementById('modal').classList.contains('open'));
 await settings();await page.locator('#syncClock').click();await page.waitForFunction(()=>!document.getElementById('modal').classList.contains('open'));
 await settings();await more();await page.locator('#monitorSettings').click();await page.locator('#monitorToggle').waitFor();await auditButtons();await page.locator('#monitorToggle').click();await page.locator('#sheetClose').click();
 await settings();await more();await page.locator('#maintenanceRead').click();assert((await page.locator('#maintenanceResult').textContent()).includes('USB_REQUIRED'));await page.locator('#sheetClose').click();
 await settings();await more();await page.locator('#chNightOff').click();await page.locator('#cieloPauseAll').click();v.chihiros_active=v.pez_verde_active=false;v.always_on=false;await sync();await page.locator('#chNightOn').click();await page.locator('#sheetClose').click();
 await page.locator('[data-view="lab"]').click();const report=page.waitForEvent('download');await page.locator('#diagDownload').click();assert.equal((await report).suggestedFilename(),'CIELO_INFORME_MAESTRO.json');
 await page.locator('#viewLab details').click();await auditButtons();
 const binary=Buffer.alloc(600000);binary[0]=0xe9;binary[3]=0x30;binary[12]=9;
 await page.locator('#otaFile').setInputFiles({name:'firmware.bin',mimeType:'application/octet-stream',buffer:binary});
 canUpdate=false;await page.locator('#otaInstall').click();await page.waitForFunction(()=>!state.otaBusy);assert(!requests.some(r=>r.path==='/api/ota/prepare'));
 canUpdate=true;await page.locator('#otaInstall').click();await page.waitForFunction(()=>!state.otaBusy&&state.otaAwaitBoot);await sync();assert(await page.locator('#otaInstall').isEnabled());
 assert.equal(requests.filter(r=>r.path==='/api/ota/upload').length,1);
 assert.deepEqual(errors,[]);
 console.log(JSON.stringify({browser:'Chromium',mock_only:true,viewports:[320,390,768],checked_buttons:[...buttons].sort(),checked_button_count:buttons.size,requests:requests.length,console_errors:errors,passed:true}));
 await browser.close();
})().catch(e=>{console.error(e);process.exit(1)});
