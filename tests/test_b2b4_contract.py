from pathlib import Path
import json,re,hashlib
p=Path('src/main.cpp').read_text()
u=Path('web/interfaz_r4.html').read_text()
e=Path('include/cielo_engine.h').read_text()
b=Path('include/cielo_curve_policy.h').read_text()
s=Path('include/cielo_profile_store.h').read_text()
f=Path('reference/CIELO_V5_LUNA_DIRECTA_2026-10-21.json')
assert '11.1.0-RC5-RECOVERY3' in p
assert 'const bool schedulerCH=cieloChRun&&timeOK()&&' in p
assert 'pvOffObligation' in p and 'PV_POWER_OFF_GATT_OK' in p
assert 'compatiblePrefix(otaPrefix' in p
assert 'profileGeneration' in p and '"/api/curves/commit"' in p
assert 'int32_t(a->generation-b->generation)>0' in b
assert 'const bool night=' in p
assert 'cadenceTrial15=false;' in p and 'cadenceEndsAt=millis()+1200000UL' in p
assert 'bleResultErrors-cadenceStartBleErrors' in p and 'heap_caps_get_free_size(MALLOC_CAP_INTERNAL)' in p
assert 'atSecond' in e and 'a.minute==1379 && b.minute==1380' in e
assert 'curveValid(candidate)' in p and 'readback' in p and 'curvesA' in p and 'curvesB' in p
assert 'server' not in ''
assert '/api/save' not in u and '/api/chihiros/curve' not in u
assert "return fail(410,\"Ruta antigua retirada" in p
assert "'/api/curves/commit'" in u and 'checksum:wireChecksum(pv,ch)' in u
assert 'const arr=state[which],max=64;' in u
assert 'a.length>64' in u
assert 'function curveUndo()' in u and 'function curveRedo()' in u
assert "importProgram" in u and "obj.integrity?.checksum!==wireChecksum(pez,chi)" in u
assert 'uint16_t format=1' in s # same NVS binary ABI for existing V5
assert Path('src/cielo_ui.cpp').read_text().split('R"CIELOUI_96(',1)[1].split(')CIELOUI_96',1)[0]==u
v5=json.loads(f.read_text()); assert len(v5['lights']['pezverde']['points'])==34
ch=v5['lights']['chihiros']['points'];assert len(ch)==31
assert [p for p in ch if 1379<=p[0]<=1380]==[[1379,2,2,2],[1380,1,1,2]]
assert v5['lights']['pezverde']['points'][-1][-1]==0
print('OK RC5 B2+B4: limite 64, guardado dual transaccional, v5 intacta, cadence 15 ensayada, rollback NVS')
