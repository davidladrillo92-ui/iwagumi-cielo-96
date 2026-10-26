"""Freeze all lighting/BLE logic against the exact RECOVERY3.6 source base."""
from pathlib import Path
import json,hashlib
f=json.loads(Path('tests/fixtures/storage_frozen_lighting_sha256.json').read_text())
s=Path('src/main.cpp').read_text()
for r in f['ranges']:
    part=s[s.index(r['start']):s.index(r['end'],s.index(r['start']))]
    assert hashlib.sha256(part.encode()).hexdigest()==r['sha256'],r['name']
for p,h in f['files'].items():assert hashlib.sha256(Path(p).read_bytes()).hexdigest()==h,p
fs=Path('include/cielo_profile_fs.h').read_text()
for mutation in ['SPIFFS.begin(true','SPIFFS.format(','esp_spiffs_format(','esp_spiffs_check(']:assert mutation not in fs
print('PASS storage scope: original lighting/BLE/recovery/maintenance/UI/partition hashes unchanged; no format/check/repair fallback')
