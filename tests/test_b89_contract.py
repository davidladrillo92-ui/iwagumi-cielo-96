from pathlib import Path
import hashlib,json,re
pins=json.loads(Path('tests/fixtures/rc6_invariants_sha256.json').read_text())
for name,digest in pins.items():
    raw=Path(name).read_bytes()
    if name=='include/cielo_ble_queue.h':
        observer=('  // Read-only telemetry: actual age since submission, including any retries.\n'
                  '  bool active_age_ms(uint32_t sequence,uint32_t now,uint32_t& age) const {\n'
                  '    if(!inflight_||active_.cmd.sequence!=sequence)return false;\n'
                  '    age=uint32_t(now-active_.at_ms);return true;\n'
                  '  }\n').encode()
        assert raw.count(observer)==1
        raw=raw.replace(observer,b'')
    assert hashlib.sha256(raw).hexdigest()==digest,name
main=Path('src/main.cpp').read_text();proxy=Path('src/cielo_https_proxy.cpp').read_text()
assert 'collectHeaders(keys,sizeof(keys)/sizeof(keys[0]))' in main
assert 'LAN_NO_PASSWORD' in main
assert '/api/monitor' not in main and 'cielo_monitor_service.h' not in main
assert not Path('monitor').exists()
assert not Path('src/cielo_monitor_service.cpp').exists()
assert 'SPIFFS.begin' not in Path('src/cielo_maintenance.cpp').read_text()
assert 'setInsecure' not in proxy
ui=Path('web/interfaz_r4.html').read_text();generated=Path('src/cielo_ui.cpp').read_text()
assert generated=='#include <Arduino.h>\n#include "cielo_ui.h"\nconst char CIELO_UI[] PROGMEM = R"CIELOUI_96('+ui+')CIELOUI_96";\n'
assert not Path('.github/workflows/build-from-zip.yml').exists()
assert not Path('CIELO_SOURCE.zip').exists()
print('PASS B8/B9: perfiles, protocolos, NVS y particiones idénticos; vigilante externo retirado; mantenimiento USB conservado')
