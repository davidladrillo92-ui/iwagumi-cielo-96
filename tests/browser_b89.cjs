// Prueba de UI contra respuestas simuladas. Chromium no equivale a Safari físico.
const fs=require('fs'),assert=require('assert');
const {chromium}=require(require.resolve('playwright',{paths:[process.env.CODEX_PRIMARY_RUNTIME_NODE_MODULES||process.cwd()]}));
(async()=>{
 const browser=await chromium.launch({headless:true});
 const page=await browser.newPage({viewport:{width:390,height:844}});const errors=[],writes=[];
 page.on('pageerror',e=>errors.push(e.message));page.on('dialog',d=>d.accept());
 const source=fs.readFileSync('web/interfaz_r4.html','utf8');
 await page.route('http://cielo.test/**',route=>{
  const url=new URL(route.request().url());let data={};
  if(url.pathname==='/')return route.fulfill({contentType:'text/html',body:source});
  if(route.request().method()==='POST')writes.push(url.pathname);
  if(url.pathname==='/api/auth/session')data={session:'a'.repeat(64)};
  if(url.pathname==='/api/monitor/status')data={enabled:false};
  if(url.pathname==='/api/monitor/token')data={token:'b'.repeat(64)};
  if(url.pathname==='/api/maintenance')data={schema:'cielo-maintenance-v1',flash_bytes:8388608,flash_backup:'USB_REQUIRED_NOT_PERFORMED'};
  if(url.pathname==='/api/status')data={rgb:[1,1,1],ble:true,clockValid:true,clock:'21/10/2026 22:00:00',firmware:'MOCK_RC5',homeIp:'192.0.2.1'};
  return route.fulfill({contentType:'application/json',body:JSON.stringify(data)});
 });
 await page.goto('http://cielo.test/');
 await page.evaluate(()=>settings());
 await page.locator('#modalBody summary').filter({hasText:'Más opciones'}).click();
 await page.locator('#monitorSettings').click();await page.locator('#monitorEnable').click();
 await page.waitForTimeout(50);await page.locator('#sheetClose').click();
 await page.evaluate(()=>settings());await page.locator('#modalBody summary').filter({hasText:'Más opciones'}).click();await page.locator('#maintenanceRead').click();
 await page.waitForFunction(()=>document.getElementById('maintenanceResult').textContent.includes('USB_REQUIRED'));
 assert.deepEqual(writes,['/api/monitor/enable']);
 assert.deepEqual(errors,[]);
 console.log('PASS Chromium 390x844: ajustes monitor, sesión, token efímero y mantenimiento; ninguna orden de luz');
 await browser.close();
})().catch(e=>{console.error(e);process.exit(1)});
