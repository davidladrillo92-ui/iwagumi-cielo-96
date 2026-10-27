#!/usr/bin/env python3
"""Static UI checks; not an optical or Bluetooth hardware test."""
from pathlib import Path
ui=Path("web/interfaz_r4.html").read_text(encoding="utf-8")
embedded=Path("src/cielo_ui.cpp").read_text(encoding="utf-8")
assert 'R"CIELOUI_96(' in embedded and ')CIELOUI_96";' in embedded
embedded=embedded.split('R"CIELOUI_96(',1)[1].rsplit(')CIELOUI_96";',1)[0]
assert embedded.strip()==ui.strip(), "Firmware UI and editable HTML must be identical"
assert "Último envío BLE" not in ui, "Old technical label remains"
assert 'function lampPresentation(' in ui and 'function renderLampStates(' in ui
assert "$(prefix+'Sub').textContent=lamp.label;$(diag).textContent=lamp.label" in ui
assert "if(linked===false&&!released&&active)return result('Sin enlace BLE')" in ui
assert 'luz real no verificada' in ui.lower()
assert 'diagLoad()' not in ui
print('PASS one lamp vocabulary, one live snapshot, shared home/Centre labels, explicit link/freshness guards and optical qualification')
