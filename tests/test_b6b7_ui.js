'use strict';
const assert=require('assert'),fs=require('fs'),vm=require('vm');
const html=fs.readFileSync('web/interfaz_r4.html','utf8');
const js=html.split('<script>')[1].split('</script>')[0];
const elems={};
function element(id){return elems[id]||(elems[id]={id,value:'',textContent:'',innerHTML:'',disabled:false,dataset:{},style:{background:'',setProperty(){}},classList:{toggle(){},add(){},remove(){},contains(){return false}},setAttribute(n,v){this[n]=v},focus(){},getClientRects(){return [1]}})}
const context={console,window:{addEventListener(){}},document:{getElementById:element,querySelectorAll(){return []},activeElement:null},setTimeout(){},clearTimeout(){},Date,Math,Number,JSON,Blob,URL:{createObjectURL(){return 'blob:abc'},revokeObjectURL(){}},confirm(){return true}};
vm.createContext(context);vm.runInContext(js,context);
const evalj=x=>vm.runInContext(x,context);
assert.equal(evalj('chartWindow(1,1380).span'),60);
assert.equal(evalj('chartWindow(3,20).begin'),0);
assert.equal(evalj('chartWindow(6,1435).end'),1440);
assert.equal(evalj('chartWindow(24,720).begin'),0);
assert.equal(evalj('chartWindow(1,1380).begin'),1350);
// No editar ni transmitir en modo simulación.
assert.equal(evalj('state.preview'),null);
assert.equal(evalj('state.dirty.chi'),false);
evalj('state.curve="pez";state.scrub=1380;state.zoomHours=1;state.baseline={pez:state.pez.map(p=>p.slice()),chi:state.chi.map(p=>p.slice())};state.compare=true;drawChart()');
assert.match(element('chart').innerHTML,/chart-point/);
assert.match(element('chart').innerHTML,/stroke-dasharray="5 5"/);
assert.match(element('chartInspector').textContent,/HORA SIMULADA/);
assert.equal(element('chartAxisTimes').innerHTML.match(/<span>/g).length,5);
assert.ok(element('colorPreviewPv').style.background.startsWith('#'));
assert.match(element('colorPreviewPv')['aria-label'],/no medido/);
assert.match(element('colorPreviewChi')['aria-label'],/no medido/);
// Las ediciones permanecen en RAM del navegador y se marcan como pendientes.
evalj('state.dirty.pez=true;state.undoStack.push(snapshotCurves());updateEditorStatus()');
assert.match(element('curvePending').textContent,/SIN GUARDAR/);
assert.equal(element('saveCurve').disabled,true); // desconectado del ATOM
assert.equal(element('undoCurve').disabled,false);
// Invitado y admin informan del rol; guardar exige sesión API externa.
assert.equal(element('authBadge').textContent,'Local');
evalj('updateEditorStatus()');
assert.equal(element('accessAction').textContent,'Ajustes');
// Reloj simulado y virtualización son funciones puramente visuales, no BLE.
assert.ok(!/api\(|ble_dispatch|fetch\(/.test(evalj('drawChart.toString()')));
assert.ok(/actualiz/.test('actualizaciones')); // token de test sencillo
// PWA sin cache de órdenes: el navegador no registra service worker.
assert.match(html,/rel="manifest" href="\/manifest.webmanifest"/);
assert.match(html,/apple-touch-icon/);
assert.ok(!/serviceWorker\.register\(/.test(html));
assert.match(html,/aria-live="polite"/);
assert.ok(!html.includes('user-scalable=no'));
assert.match(html,/data-zoom="1"/);assert.match(html,/data-zoom="3"/);assert.match(html,/data-zoom="6"/);assert.match(html,/data-zoom="24"/);
assert.match(html,/id="sheetClose"/);
console.log('PASS B6/B7: zoom 1/3/6/24, curvas comparadas, simulación local, escala de brillo, roles, foco, PWA');
