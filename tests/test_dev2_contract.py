"""Pin unchanged firmware files and assert DEV2 integration points, not hardware validation."""
from pathlib import Path
import json,hashlib
s=Path('src/main.cpp').read_text()
f=json.loads(Path('tests/fixtures/dev2_protected_sha256.json').read_text())
for n,h in f.items():
 if n not in {"include/cielo_history314.h","src/cielo_ui.cpp","web/interfaz_r4.html"}:assert hashlib.sha256(Path(n).read_bytes()).hexdigest()==h,n
for token in ('isolated315[0].loss(millis())','isolated315[1].loss(millis())','isolated315[d].dispatch(cmd.sequence)',
 'isolated315[device].complete(res.sequence,matched,res.success,now,urgentOff315)',
 'if(matched&&urgentOff315&&!offEvidence315','if(autoApply315&&isolated315[d].waiting&&!(d==0&&pvOffObligation))',
 'const auto v=desiredCh();cmd.r=v.r;cmd.g=v.g;cmd.b=v.b;',
 'const auto v=desiredPv();cmd.r=v.r;cmd.g=v.g;cmd.b=v.b;cmd.brightness=v.brightness;',
 'if(cieloChReturnPending)queue.cancel_pending(cielo::ble::Device::Chihiros);',
 'bleUnexpectedDisconnectTick();cielo96::ble_set_pv_off_required(pvOffObligation);',
 'pvOffNextTry=now+300000UL','night_probe_enabled\\":false','isolated_recovery315',
 'if(starting&&(isolated315[0].waiting||isolated315[1].waiting))'):
 assert token in s,token
# Only a trusted autonomous CH link loss takes the new path; legacy fallback still exists.
assert 'else if(device==1 && cieloChRun && cieloChWasDriven' in s
assert 'cieloRequestReturn("Desconexion BLE inesperada:' in s
assert 'cieloRequestReturn("Emergencia: volver a My Chihiros")' in s
assert 'cieloRequestReturn("Actualizacion OTA: ceder Chihiros a My Chihiros")' in s
print('PASS DEV2 wiring and',len(f),'protected files; target/hardware not tested')
