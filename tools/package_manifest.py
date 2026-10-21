#!/usr/bin/env python3
"""Manifiesto de compilación; una huella NO sustituye firma ECDSA de OTA."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
p=argparse.ArgumentParser();p.add_argument('binary');p.add_argument('output');a=p.parse_args()
raw=Path(a.binary).read_bytes()
if not raw or len(raw)>0x330000 or raw[0]!=0xe9:raise ValueError('Imagen de aplicación inválida o demasiado grande')
# esp_image_header_t.chip_id a offset 12 (ESP32-S3=9).
if int.from_bytes(raw[12:14],'little')!=9:raise ValueError('El BIN no identifica ESP32-S3')
m={'schema':'cielo-build-v1','commit':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
   'version':'11.1.0-RC5-B1-B9-MONITOR','board':'ATOMS3 Lite C124 / ESP32-S3',
   'application_bytes':len(raw),'application_sha256':hashlib.sha256(raw).hexdigest(),
   'ota_signature':'NOT_SIGNED_PRIVATE_KEY_REQUIRED','hardware_validation':'PENDING','installed_confirmed':'RC3',
   'v5_reference':{'pv':34,'ch':31,'fnv1a32':'5814359c'},
   'immutable':json.loads(Path('tests/fixtures/rc5_b7_immutable_sha256.json').read_text())}
Path(a.output).write_text(json.dumps(m,indent=2)+'\n')
