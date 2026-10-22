from pathlib import Path
import json,re
ui=Path('web/interfaz_r4.html').read_text()
main=Path('src/main.cpp').read_text()
header=Path('include/cielo_pwa_assets.h').read_text()
assert '11.1.0-RC5-B1-B9-SIMPLE1' in main
assert 'srv.on("/manifest.webmanifest"' in main
assert 'srv.on("/icon-180.png"' in main
assert 'srv.on("/icon.svg"' in main
assert 'cache de comandos' in main
assert 'CIELO_MANIFEST' in header and 'CIELO_ICON_180_PNG' in header
assert '"display":"standalone"' in header
assert 'serviceWorker.register' not in ui
assert 'id="chartAxisTimes"' in ui and 'id="chartZoomNote"' in ui
assert 'function chartWindow(' in ui and 'function chartPointSelect(' in ui
assert 'function nudgePoint(' in ui and 'function updateEditorStatus(' in ui
assert 'function dialogKeydown(' in ui and 'aria-label="Cerrar panel"' in ui
assert 'id="authBadge"' not in ui and 'function settings(' in ui
assert 'HORA SIMULADA' in ui and 'no medido' in ui
assert 'dimmedPv[k]*pezWeight' in ui
assert 'role="img"' in ui and 'prefers-reduced-motion' in ui
assert 'function wireChecksum(' in ui and '/api/curves/commit' in ui
assert '0x03' in main or 'ble_custom_resume_request' in main
assert 'CIELO_ENABLE_SHORT_HOST_ALIAS' not in main # sin alterar certificado/OTA
ref=Path('reference/CIELO_V5_LUNA_DIRECTA_2026-10-21.json')
p=json.loads(ref.read_text())
assert len(p['lights']['pezverde']['points'])==34 and len(p['lights']['chihiros']['points'])==31
assert [1379,2,2,2] in p['lights']['chihiros']['points'] and [1380,1,1,2] in p['lights']['chihiros']['points']
print('PASS B6/B7 contratos: PWA sin SW, permisos, curva V5 intacta, zoom y gráficos honestos')

assert 'pv_last_gatt_age_sec' in main and 'ch_last_gatt_age_sec' in main
assert 'id="lastConfirmedPv"' not in ui and 'id="lastConfirmedChi"' not in ui
