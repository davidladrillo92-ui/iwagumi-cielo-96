from pathlib import Path
import hashlib,json,re
pins=json.loads(Path('tests/fixtures/rc6_invariants_sha256.json').read_text())
for name,digest in pins.items():assert hashlib.sha256(Path(name).read_bytes()).hexdigest()==digest,name
main=Path('src/main.cpp').read_text();svc=Path('src/cielo_monitor_service.cpp').read_text();proxy=Path('src/cielo_https_proxy.cpp').read_text()
assert 'collectHeaders(keys,sizeof(keys)/sizeof(keys[0]))' in main
assert 'LAN_NO_PASSWORD' in main
assert 'if(!adminGuard())return;' in svc and 'if(!privateGuard())return;' in svc
assert 'sample().ota' in svc and 'putBool("mon_on",value)' in svc
assert 'LAN_NO_PASSWORD' in svc and 'credentialHash' not in svc
assert 'now-lastPoll<15000UL' in svc
assert 'queue.submit' not in svc and 'ble_dispatch' not in svc
assert 'SPIFFS.begin' not in Path('src/cielo_maintenance.cpp').read_text()
assert 'setInsecure' not in proxy
ui=Path('web/interfaz_r4.html').read_text();generated=Path('src/cielo_ui.cpp').read_text()
assert generated=='#include <Arduino.h>\n#include "cielo_ui.h"\nconst char CIELO_UI[] PROGMEM = R"CIELOUI_96('+ui+')CIELOUI_96";\n'
assert not Path('.github/workflows/build-from-zip.yml').exists()
assert not Path('CIELO_SOURCE.zip').exists()
print('PASS B8/B9: perfiles, protocolos, NVS y particiones idénticos; monitor de lectura y credenciales aisladas')
