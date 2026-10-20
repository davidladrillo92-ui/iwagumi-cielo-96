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
assert 'lightingTask();' not in code # arranque BLE no programa luces
assert 'static bool labTxEnabled()' in code
for guarded in ('static void submitPv(', 'static void submitCh(', 'static void requestAuto('):
    section=code[code.index(guarded):code.index('}',code.index(guarded))]
    assert 'if(!labTxEnabled())' in section,guarded
assert 'lab_manual' in code and 'LAB_TX_BLOQUEADA' in code
assert 'ble_scan_request()' in code and 'ble_scan_result(result)' in code
assert 'lab.running' in code and 'profileValid(profile)' in code
assert 'j+=",\\\"lab\\\":"+labJson();' in code
assert 'LAB_TX_WINDOW_MS=90000UL' in code
bleworker=(r/'src/cielo_ble_worker.cpp').read_text()
assert 'scanPassive()' in bleworker and 'scan->setActiveScan(false)' in bleworker
assert 'BLEDevice::init(' in bleworker
assert 'sendPez(c,err):sendCh(c,err)' in bleworker
assert 'scanPassive();lastHeartbeat=millis();' in bleworker
assert code.index('if(!sleepSet || !wifiCoexistReady())') < code.index('bleReady=cielo96::ble_begin();')
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
assert '-DCIELO_ENABLE_OTA=1' not in (r/'platformio.ini').read_text()

assert '<title>CIELO 9.6 R4.4 · Laboratorio</title>' in actual
assert 'id="viewLab"' in actual and 'data-view="lab"' in actual
for identifier in ('labStart','labScan','labBrowser','labArm','labAuto','labReport','labMetrics'):
 assert ('id="'+identifier+'"') in actual,identifier
assert "n==='lab'?'Lab'" in actual
# JavaScript embebido: sintaxis comprobada con Node en GitHub Actions.
import tempfile
with tempfile.NamedTemporaryFile('w',suffix='.js',delete=False,encoding='utf-8') as js:
 js.write(actual.split('<script>',1)[1].split('</script>',1)[0])
 js_path=js.name
try:
 subprocess.run(['node','--check',js_path],check=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
finally:
 Path(js_path).unlink(missing_ok=True)
assert 'CIELO 9.6 R4.4 · laboratorio experimental' in actual
for route in ('/api/status','/api/save','/api/chihiros/curve','/api/manual',
              '/api/chihiros/manual','/api/chihiros/stop','/api/ota/upload',
              '/api/diagnostico/maestro','/api/service/status'):
 assert route in actual and route in (r/'src/main.cpp').read_text(),route
flash=Path('/mnt/data/FLASH_ATOM_8MB_COMPLETA_PRIVADA.bin')
if flash.exists():
 d=flash.read_bytes();assert len(d)==8388608
 assert hashlib.sha256(d).hexdigest()=='b3362b4169ec81b635fec6eaf242167a19100aa1682518cc9275ee459e7355f1'
 assert d[0x10002]==0x02 and d[0x10003]==0x3f
 assert d[0x8000:0x8002]==b'\xaa\x50'
print('OK interfaz original, endpoints implementados, tabla y cabecera bootloader flash original')
for name in ('test_codec_and_profile','test_ble_queue'):
 out=r/'tests'/name
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+str(r/'include'),str(r/'tests'/f'{name}.cpp'),'-o',str(out)],check=True)
 subprocess.run([str(out)],check=True)
print('Pruebas nativas correctas; compilacion ESP32-S3 pendiente de toolchain y hardware')
