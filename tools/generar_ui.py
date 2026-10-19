#!/usr/bin/env python3
from pathlib import Path
r=Path(__file__).resolve().parents[1]
s=(r/'web/interfaz_r4.html').read_text(encoding='utf-8')
assert ')CIELOUI_96"' not in s
assert '<!DOCTYPE html>' in s and '</html>' in s
(r/'src/cielo_ui.cpp').write_text('#include <Arduino.h>\n#include "cielo_ui.h"\nconst char CIELO_UI[] PROGMEM = R"CIELOUI_96('+s+')CIELOUI_96";\n',encoding='utf-8')
print('UI',len(s))
