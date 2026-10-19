#!/usr/bin/env python3
from pathlib import Path
import re, json, hashlib, subprocess,sys
r=Path(__file__).resolve().parents[1]
original=(r/'reference/INTERFAZ_ORIGINAL_953_SIN_MODIFICAR.html').read_text()
actual=(r/'web/interfaz_r4.html').read_text()
assert 'IWAGUMI CIELO' in original and len(original)>70000
assert '/api/updates/' not in actual and 'workers.dev' not in actual
assert 'Actualizar CIELO · desde GitHub' not in actual
assert '<button id="otaInstall" disabled>' in actual
assert 'Descargar JSON' in actual
assert '<title>CIELO 9.6 R4.2 · Desarrollo</title>' in actual
assert 'CIELO 9.6 R4.2 · firmware de desarrollo' in actual
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
