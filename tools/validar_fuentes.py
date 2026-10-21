#!/usr/bin/env python3
from pathlib import Path
import re, json, hashlib, subprocess,sys,os,time,datetime
r=Path(__file__).resolve().parents[1]
original=(r/'reference/INTERFAZ_ORIGINAL_953_SIN_MODIFICAR.html').read_text()
actual=(r/'web/interfaz_r4.html').read_text()
assert 'IWAGUMI CIELO' in original and len(original)>70000
assert '/api/updates/' not in actual and 'workers.dev' not in actual
assert 'Actualizar CIELO · desde GitHub' not in actual
assert '<button id="otaInstall" disabled>' in actual
assert 'Descargar JSON' in actual
code=(r/'src/main.cpp').read_text()
assert 'ESP_RST_PANIC' in code and 'reset_reason_name' in code
assert 'boot_marked_healthy' in code and 'latencyP95' in code
assert 'configTzTime(CIELO_TZ' in code and 'configTime(0,0' not in code
assert 'WiFi.setSleep(false)' not in code
assert 'WiFi.setSleep(true)' in code and 'wifiCoexistReady()' in code
assert 'BLE_COEX_BLOQUEADO' in code and 'wifi_modem_sleep' in code
assert 'COEX_AVISO' not in code and 'esp_wifi_set_ps(WIFI_PS_MIN_MODEM)' in code
assert 'lightingTask();' in code # RC4: activo solo mediante boton, nunca desde BLE por si mismo
assert 'static bool cieloPvRun=false,cieloChRun=false' in code
assert 'if(!cieloPvRun && !cieloChRun && !cieloChWasDriven && !pvOffObligation && !pvControlDesired)return;' in code
assert 'cielo::at(profile.pv,profile.pvCount,minuteOfDay())' not in code or 'desiredPv()' in code
assert 'const bool chWindow=true' in code
assert 'const bool schedulerCH=cieloChRun&&timeOK()&&' in code
assert 'v5_always' in code and 'cieloResumePending' in code
assert 'V5_AUTO_REANUDADO' in code
assert 'cieloTrendTick();' in code and 'cieloTrendJson()' in code
assert 'daily_monitoring' in code
assert 'WiFi.scanNetworks(true,false)' in code
assert '/api/wifi/scan' in code and '/api/wifi/networks' in code
assert 'pv_online_recent' in code and 'ch_online_recent' in code
assert 'avisos_historicos' in code and 'incidencia_actual' in code
assert '23:00: finalizar Natural V4 Chihiros' not in code
assert 'lastPvDesiredAt' in code and 'lastChDesiredAt' in code
assert 'queue.cancel_pending(cielo::ble::Device::Chihiros)' in code
assert 'cielo96::ble_custom_resume_request()' in code
assert 'cieloReturnTries<2' in code and 'V4_RETORNO_GATT_OK' in code
assert 'PV_DEGRADADA' in code and 'V5_24H_ACTIVADA' in code
assert 'cieloChRun&&timeOK()' in code
assert '/api/cielo/v4/start' in code and '/api/cielo/v4/return' in code and '/api/cielo/v4/pause' in code
assert '!cieloPvRun && !cieloChRun && !cieloChReturnPending && !cieloChAwaitResponse' in code

assert 'static bool labTxEnabled()' in code
for guarded in ('static void submitPv(', 'static void submitCh('):
    section=code[code.index(guarded):code.index('}',code.index(guarded))]
    assert 'if(!labTxEnabled())' in section,guarded
assert 'lab_manual' in code and 'LAB_TX_BLOQUEADA' in code
assert 'ble_scan_request()' in code and 'ble_scan_result(result)' in code
assert 'lab.running' in code and 'profileValid(profile)' in code
assert 'srv.hasArg("share")?String("null"):labJson()' in code
assert 'LAB_TX_WINDOW_MS=90000UL' in code
assert 'parseBoundedUInt(srv.arg("avg"),avg,60000)' in code
assert 'parseBoundedUInt(srv.arg("max"),maximum,60000)' in code
assert 'positiveInt(srv.arg("avg"),avg)' not in code
assert 'Cola BLE abortada: autorizacion caducada' in code
bleworker=(r/'src/cielo_ble_worker.cpp').read_text()
assert 'scanDiscover()' in bleworker and 'scan->setActiveScan(true)' in bleworker
assert 'bleNameMatches' in bleworker and 'cielo_ble_match.h' in bleworker
assert 'const std::string name=d.haveName()?d.getName():std::string();' in bleworker
assert 't.addressType=d.getAddressType()' in bleworker
assert 't.client->connect(address,t.addressType)' in bleworker
assert 'rec.nus=d.isAdvertisingService(BLEUUID(codec::CH_SVC));' in bleworker
assert 'r.observed_count<8' in bleworker
assert 'lab.lastScan.observed_count' in code and 'nearby' in code
assert 'res.action==static_cast<uint8_t>(cielo::ble::Action::RestoreAuto)' in code
assert 'CH_AUTO_ERROR' in code and 'CH_AUTO_NO_ENVIADO' in code
assert 'CH_AUTO_INSEGURO' in code
assert 'CH_AUTO_MODE_UNVERIFIED' in bleworker
assert 'if(!put(t,frame,size,err))return false;\n  t.manualEntered=false' not in bleworker
assert 'AUTO Chihiros bloqueado' in code
assert 'cielo96::ble_probe_request(index)' in code
assert 'cielo96::ble_probe_result(probe)' in code
assert 'gatt_probe' in code
assert 'probeGATT(uint8_t device)' in bleworker
assert 'if(t.client && t.client->isConnected())t.client->disconnect();' in bleworker
assert 'ble_probe_request(uint8_t device)' in bleworker
assert 'No se envio comando AUTO' not in code
assert "function labAuto()" not in actual
assert 'discovery_scan' in code and 'named_advertisements' in code
assert 'scan_duration_requested_sec' in code
assert '11.1.0-RC5-B1B5B3-B2B4-PERFILES' in code
assert 'CH_CONTRASTE_INICIO' in code
assert 'CH_CONTRASTE_30_GATT_OK' in code
assert 'CH_CONTRASTE_CADUCADO' in code
assert 'chihiros_contrast' in code and '/api/chihiros/contrast/start' in code
assert 'contrastStage!=2' in code and 'contrastStage=4' in code
assert 'chiCieloNight=false;chiManual=true' in code
assert 'if(queue.busy()||contrastStage==1||contrastStage==4)' in code
assert "30,30,30,0,millis(),45000" in code

assert 'ble_release_request(index)' in code and '/api/ble/release' in code
assert 'releaseIdleLinks()' in bleworker and 'BLE_IDLE_RELEASE_MS=5000UL' in bleworker
assert 'ble_link_status(linkIndex)' in code
assert '/api/lab/preview' in code and 'CURVA_PREVIA_CH' in code
assert 'chihiros_auto_verified' in code and 'CH_AUTO_MODE_UNVERIFIED' in bleworker
assert 'BLE_AUTO_SOLICITADO' in code and 'millis()-bootMs>=30000UL' in code
assert 'ble_start_requires_button\\\\":false' in code or 'ble_start_requires_button\\":false' in code
assert 'if(!labTxEnabled())' in code
assert 'BLEDevice::init(' in bleworker
assert 'sendPez(c,err):sendCh(c,err)' in bleworker
assert 'scanDiscover();lastHeartbeat=millis();' in bleworker
assert code.index('if(!wifiCoexistReady())WiFi.setSleep(true);') < code.index('bleReady=cielo96::ble_begin();')
assert 'RTC_NOINIT_ATTR' in code and 'previous_reset_phase_valid' in code
assert 'phone_sync_this_boot' in code and 'utc_offset' in code
# Comprobar el cambio invierno/verano de la regla TZ sin depender de la hora del runner.
oldtz=os.environ.get('TZ')
try:
 os.environ['TZ']='CET-1CEST,M3.5.0,M10.5.0/3'
 time.tzset()
 cases=[
   (datetime.datetime(2026,10,20,9,tzinfo=datetime.timezone.utc),11,1),
   (datetime.datetime(2026,11,1,9,tzinfo=datetime.timezone.utc),10,0),
   (datetime.datetime(2027,4,1,9,tzinfo=datetime.timezone.utc),11,1),
 ]
 for utc,expected_hour,expected_dst in cases:
  local=time.localtime(utc.timestamp())
  assert (local.tm_hour,local.tm_isdst)==(expected_hour,expected_dst),(utc,local)
finally:
 if oldtz is None: os.environ.pop('TZ',None)
 else: os.environ['TZ']=oldtz
 time.tzset()
assert 'WiFi.softAP("CIELO96-CONFIG",apPassword.c_str())' in code
assert 'WiFi.softAP("CIELO96-RESCATE")' in code
assert 'if(rescueOpen)return fail(403' in code
assert 'statusPixel' in code and 'CIELO_LED_GPIO=35' in code
assert 'bleReady=false; // Do not start BLE' in code
assert code.index('configureRoutes();') < code.index('bleReady=cielo96::ble_begin();')
assert 'CIELO_ENABLE_OTA 1' in code and 'physicalOtaGrant()' in code
assert 'X-Cielo-Sha256' in code and 'otaDigestOut' in code
assert 'if(rescueOpen)return fail(403' in code
assert 'Update.begin' in code and 'Update.end(true)' in code

assert '<title>CIELO 11 · Iwagumi en vivo</title>' in actual
assert 'id="viewLab"' in actual and 'data-view="lab"' in actual
for identifier in ('labGuided','heroTint','chiLed','pezLed','scrubHint','otaProgress','diagView','diagDownload','otaInstall'):
 assert ('id="'+identifier+'"') in actual,identifier
for removed in ('labStart','labScan','labBrowser','labArm','labProbePv','labProbeCh','labCh10','labCh20','labCh35','labReport','labMetrics'):
 assert ('id="'+removed+'"') not in actual,removed
# Causa R4 GET/POST: metodo POST se envia con {}.
assert "await api('/api/lab/start',{});" in actual
assert "await api('/api/lab/scan',{});" in actual
assert "await api('/api/lab/stop',{});" in actual
assert "$('labState')" not in actual
assert "function labPretty(d)" in actual and "$('cieloLinks')" not in actual
assert "id=\"cielo95Diagnostics\"" in actual
assert actual.count('id="cielo95Diagnostics"')==1
assert actual.count('id="diagDownload"')==1
assert actual.count('id="labGuided"')==1
assert 'id="viewLab"' in actual and 'id="viewLight"' in actual and 'id="viewCurves"' in actual
assert "n==='lab'?'Lab'" in actual
assert 'id="viewService"' not in actual and 'data-view="service"' not in actual
assert 'id="cieloLinks"' not in actual and 'id="labGuided"' in actual
assert 'id="releaseCh"' not in actual and 'id="releasePv"' not in actual
assert 'id="curveChEvening"' not in actual
assert 'function labGuided()' in actual and 'function previewNatural' not in actual
assert 'function releaseLight' in actual
assert "fetch(full?'/api/diagnostico/maestro?download=1'" in actual and "X-Cielo-Session" in actual
# JavaScript embebido: sintaxis comprobada con Node en GitHub Actions.
import tempfile
with tempfile.NamedTemporaryFile('w',suffix='.js',delete=False,encoding='utf-8') as js:
 js.write(actual.split('<script>',1)[1].split('</script>',1)[0])
 js_path=js.name
try:
 subprocess.run(['node','--check',js_path],check=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
finally:
 Path(js_path).unlink(missing_ok=True)
assert 'CIELO 11 · Iwagumi en vivo' in actual
assert 'id="heroTint"' in actual
assert 'id="chiLed"' in actual and 'id="pezLed"' in actual
assert 'id="scrubHint"' in actual and 'id="wifiNetworks"' in actual
assert "await api('/api/wifi/scan',{})" in actual
assert "await api('/api/wifi/networks')" in actual
assert 'new Option(' in actual and "if(state.preview===null)" in actual
assert 'XMLHttpRequest()' in actual and 'upload.onprogress' in actual
assert 'id="otaProgress"' in actual
assert 'Activar CIELO · siempre' in actual
assert 'id="cieloPauseAll"' in actual
# P0: toda ruta POST mutable queda protegida por la misma clave OTA.
for protected_path in ['/api/lab/start','/api/lab/stop','/api/lab/scan','/api/lab/probe','/api/cielo/v4/start','/api/cielo/v4/return','/api/cielo/v4/pause','/api/chihiros/contrast/start','/api/chihiros/contrast/cancel','/api/chihiros/my-curve-test','/api/ble/release','/api/lab/preview','/api/lab/tx/arm','/api/lab/tx/disarm','/api/lab/browser-sample','/api/clock','/api/save','/api/reset','/api/chihiros/curve','/api/chihiros/curve/reset','/api/manual','/api/chihiros/manual','/api/chihiros/stop','/api/chihiros/evening/off','/api/chihiros/evening/on','/api/chihiros/live/stop','/api/wifi/scan','/api/wifi','/api/wifi/forget']:
 assert 'srv.on("'+protected_path+'",HTTP_POST,[]{if(!requireAdminWrite())return;' in code, protected_path
assert 'X-Cielo-Key' in actual and 'requireAdminWrite()' in code

assert 'Activar CIELO · siempre' in actual
# El boton sigue existiendo, pero se ha trasladado a Emergencia con otro titulo.
assert 'id="chNightOff"' in actual
assert 'id="cieloPauseAll"' in actual
assert 'emergencyControls' in actual
assert 'async function otaPreflightUi()' in actual
assert 'async function otaPrepareLights(key)' in actual
assert "otaPost('/api/ota/prepare',key)" in actual
assert 'otaPreparationTick();' in code
assert 'otaCandidateBoot' in code
assert 'otaBleHealthy()' in code
assert 'enum class BleWorkerPhase' in (r/'include/cielo_ble_worker.h').read_text()
assert '/api/ota/prepare/status' in code
assert '/api/ota/preflight' in code
assert 'otaBleHealthy()' in code
assert "await api('/api/cielo/v4/start',{confirm:'ACTIVAR_NATURAL_V4'})" in actual
assert "await api('/api/cielo/v4/return',{confirm:'VOLVER_MY_CHIHIROS'})" in actual
assert "await api('/api/cielo/v4/pause',{confirm:'PAUSAR_TODO'})" in actual
assert "await api('/api/chihiros/contrast/start',{confirm:'ENSAYO_30_30_30'})" in actual
assert "await api('/api/chihiros/my-curve-test',{confirm:'PROBAR_CURVA_MY_CHIHIROS'})" in actual
assert 'id="restoreCustomTest"' not in actual
assert 'id="contrast30"' not in actual
assert 'id="releaseCh"' not in actual
assert 'function testMyChihirosCurve()' in actual
assert 'async function contrast30()' in actual

assert 'function testMyChihirosCurve()' in actual
assert '/api/chihiros/my-curve-test' in actual and '/api/chihiros/my-curve-test' in code
assert 'PROBAR_CURVA_MY_CHIHIROS' in actual and 'PROBAR_CURVA_MY_CHIHIROS' in code
assert 'ble_custom_resume_result' in code and 'ch_custom_curve_resume' in bleworker
assert 'customResumeLast' in code and 'chihiros_my_curve' in code
assert 'ota_enabled\\":false' not in code
# Cache de direccion en desconexion voluntaria evita escaneo de 5s por cada prueba.
release_body=bleworker.split('void releaseTarget(uint8_t device,bool userRequested){',1)[1].split('void releaseIdleLinks(){',1)[0]
assert 't.addressKnown=false' not in release_body
assert 't.addressKnown=false' in bleworker.split('bool connect(Target& t,const char*&err){',1)[1]

# La SHA-256 JavaScript debe coincidir con OpenSSL/Node: prueba obligatoria
# porque Safari HTTP no expone crypto.subtle en un origen no seguro.
sha_js=actual.split('function sha256Local(data){',1)[1].split('async function sha256Firmware(',1)[0]
sha_code='function sha256Local(data){'+sha_js
vectors=r"""
const crypto=require('crypto');
for(const s of ['', 'abc', 'CIELO-10', 'Hola\u00e1', '0'.repeat(100000)]){
 const b=Buffer.from(s,'utf8');
 const ref=crypto.createHash('sha256').update(b).digest('hex');
 const got=sha256Local(b);
 if(ref!==got){throw Error('SHA256 web mismatch '+s.slice(0,10)+' '+got+' != '+ref)}
}
console.log('SHA256 UI verificado con Node');
"""
subprocess.run(['node','-e',sha_code+'\n'+vectors],check=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)

for route in ('/api/status','/api/profile/transaction','/api/cielo/interval','/api/manual',
              '/api/chihiros/manual','/api/ble/release','/api/chihiros/my-curve-test','/api/ota/upload',
              '/api/diagnostico/maestro','/api/service/status'):
 assert route in actual and route in (r/'src/main.cpp').read_text(),route
flash=Path('/mnt/data/FLASH_ATOM_8MB_COMPLETA_PRIVADA.bin')
if flash.exists():
 d=flash.read_bytes();assert len(d)==8388608
 assert hashlib.sha256(d).hexdigest()=='b3362b4169ec81b635fec6eaf242167a19100aa1682518cc9275ee459e7355f1'
 assert d[0x10002]==0x02 and d[0x10003]==0x3f
 assert d[0x8000:0x8002]==b'\xaa\x50'
print('OK interfaz original, endpoints implementados, tabla y cabecera bootloader flash original')
for name in ('test_codec_and_profile','test_ble_queue','test_ble_match'):
 out=r/'tests'/name
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(r/'include'),str(r/'tests'/f'{name}.cpp'),'-o',str(out)],check=True)
 subprocess.run([str(out)],check=True)
print('Pruebas nativas correctas; compilacion ESP32-S3 pendiente de toolchain y hardware')

# CIELO RC4: puerta HTTPS, PBKDF2 y autenticidad de firma obligatorios.
assert 'cielo96::https_begin(prefs)' in code
assert 'srv.client().remoteIP()!=IPAddress(127,0,0,1)' in code
assert 'mbedtls_pkcs5_pbkdf2_hmac' in code and 'auth_salt' in code
assert 'verifyFirmwareSignature(otaDigestOut' in code
assert code.index('verifyFirmwareSignature(otaDigestOut') < code.index('Update.end(true)')
assert 'id="otaSignatureFile"' in actual
assert "x.setRequestHeader('X-Cielo-Signature',signature);" in actual

# RC5 B1: seguridad nocturna no equivale a confirmacion optica.
assert 'PV_POWER_OFF_INTENTO' in code and 'PV_POWER_OFF_REINTENTO' in code
assert 'PV_POWER_OFF_GATT_OK' in code and 'pv_optically_confirmed' in code
assert 'PV_RGB_OBSOLETO_BLOQUEADO' in code and 'pvOffInFlightSeq' in code
assert 'PV_RECONECTANDO' in code and 'pvOffObligation' in code

# RC5 B5: diagnóstico real, no inventar ACK ópticos o la p95 sin muestras.
assert 'iwagumi-cielo-96-diagnostico-v3' in code
assert 'fragmentation_pct' in code and 'heap_minimum' in code
assert 'queue_depth' in code and 'worker_queue_depth' in code
assert 'phase_timing_ms' in code and 'ble_transport_stats(i)' in code
assert 'pv_off_last_gatt_ms' in code and 'professional' in code
assert 'points_pv' in code and 'points_ch' in code
assert 'persistent_diagnostics' in code and 'diag_journal' in code and 'boot_hist' in code
assert 'share_safe' in code and 'srv.hasArg("share")' in code
assert 'flushJournal();' in code and 'importantEvent(name)' in code
assert 'canReportP95' in code and 'fragmentationPct' in code
assert 'diagnostic_v3' not in code  # name actually uses hyphens
assert 'id="serviceRefresh"' not in actual
assert 'id="diagShare"' in actual and '?share=1&download=1' in actual
assert 'diagReportFetchedAt' in actual and 'if(state.view===\'lab\'||$(\'diagReportText\'))' in actual
assert 'ble_request_queue_depth()' in (r/'src/cielo_ble_worker.cpp').read_text()
assert 'metricTime(metrics.write_last' in (r/'src/cielo_ble_worker.cpp').read_text()

# RC5 B2 + B4: una transaccion doble, 64 puntos, ensayos de 15s voluntarios.
assert 'saveBothCurves()' in code and 'if(generation!=profile.generation)' in code
assert 'curveCrc32' in code and 'if(!saveProfile(candidate))' in code
assert 'cieloCurveIntervalSec=30; // 15s' in code
assert 'cielo::atSecond(profile.pv' in code and 'cielo::atSecond(profile.ch' in code
assert "a.length>64" in actual and "max=64" in actual
assert 'crc32Ascii' in actual and '/api/profile/transaction' in actual
assert 'curveHistory' in actual and 'curveRemember' in actual
