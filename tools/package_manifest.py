#!/usr/bin/env python3
"""Manifiesto reproducible de la candidata LAN sin firma obligatoria."""
import argparse
import hashlib
import json
import re
import os
from pathlib import Path
import subprocess
p=argparse.ArgumentParser();p.add_argument('binary');p.add_argument('output');a=p.parse_args()
raw=Path(a.binary).read_bytes()
if not raw or len(raw)>0x330000 or raw[0]!=0xe9:raise ValueError('Imagen de aplicación inválida o demasiado grande')
# esp_image_header_t.chip_id a offset 12 (ESP32-S3=9).
if int.from_bytes(raw[12:14],'little')!=9:raise ValueError('El BIN no identifica ESP32-S3')
source=Path('src/main.cpp').read_text()
match=re.search(r'\bVERSION="([^"]+)"',source)
if not match:raise ValueError('Versión del firmware no identificada')
m={'schema':'cielo-build-v1','commit':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
   'source_branch':os.environ.get('GITHUB_HEAD_REF') or os.environ.get('GITHUB_REF_NAME') or 'local',
   'version':match.group(1),'board':'ATOMS3 Lite C124 / ESP32-S3',
   'application_bytes':len(raw),'application_sha256':hashlib.sha256(raw).hexdigest(),
   'littlefs_probe_policy':{'read_only':True,'format_if_mount_failed':False,'grow_on_mount':False,'curve_writes_enabled':False},
   'littlefs_write_policy':{'enabled_after_ro_probe':True,'format_if_mount_failed':False,'grow_on_mount':False,'preserve_nvs_slots':True,'max_preserved_stages':64,'reserve_bytes':32768,'unknown_files_never_removed':True},
   'wifi_config_policy':{'record_bytes':112,'backend':'NVS_WIFI_AB','sdk_storage':'RAM','boot_migration_writes':False,'legacy_keys_preserved':True,'unknown_schema_blocks_fallback':True,'physical_power_cuts_tested':False},
   'ota_signature':'NOT_REQUIRED_LAN_OWNER_REQUEST','hardware_validation':'PENDING','installed_confirmed':'11.1.0-RC5-RECOVERY3.11-CURVES-DESIGN-CANDIDATE (informe95,31/10/2026 14:08:12 Madrid; boot142; uptime342157s; FS gen14/37PV/44CH; esta candidata3.12 NO instalada)',
   'diagnostic_evidence_policy':{'schema_version':1,'profile_wire_checksum':'fnv1a32','p95_window_operations':32,'operational_history_ram_events':16,'extra_flash_writes':False,'lighting_engine_unchanged':True},
   'v5_reference':{'pv':34,'ch':31,'fnv1a32':'5814359c'},
   'stack_report':json.loads(Path('dist/STACK_REPORT.json').read_text()),
   'evidence_issue':'https://github.com/davidladrillo92-ui/iwagumi-cielo-96/issues/35',
   'ble_transport':'Arduino Bluedroid 2.0.17 / 8000ms connect, late-event quarantine',
   'immutable':json.loads(Path('tests/fixtures/rc6_invariants_sha256.json').read_text())}
Path(a.output).write_text(json.dumps(m,indent=2)+'\n')
