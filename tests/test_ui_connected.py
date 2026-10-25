#!/usr/bin/env python3
"""Static UI checks; not an optical or Bluetooth hardware test."""
from pathlib import Path
ui=Path("web/interfaz_r4.html").read_text(encoding="utf-8")
embedded=Path("src/cielo_ui.cpp").read_text(encoding="utf-8")
assert 'R"CIELOUI_96(' in embedded and ')CIELOUI_96";' in embedded
embedded=embedded.split('R"CIELOUI_96(',1)[1].rsplit(')CIELOUI_96";',1)[0]
assert embedded.strip()==ui.strip(), "Firmware UI and editable HTML must be identical"
assert "Último envío BLE" not in ui, "Old technical label remains"
assert "recent?'Conectado'" in ui, "Home must require recent GATT"
assert "level==='CORRECTO'&&recent&&(linked||released)?'Conectado'" in ui, "Centre must require recent GATT"
assert "Sin comunicación reciente" in ui, "Stale GATT must not appear connected"
assert "Conectado significa comunicación BLE/GATT reciente" in ui
assert "luz real no verificada" in ui.lower(), "Optical status must not be implied"
print("PASS: connected UI labels are freshness-gated and optically qualified")
