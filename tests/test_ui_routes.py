"""All API routes retained by the browser must have a firmware handler."""
from pathlib import Path
import re
html=Path('web/interfaz_r4.html').read_text();firmware=Path('src/main.cpp').read_text()+Path('src/cielo_monitor_service.cpp').read_text()
routes=set(re.findall(r'''["'](/api/[^"'?]*)''',html))
for route in routes:
 if route=='/api/monitor/':
  assert '/api/monitor/enable' in firmware and '/api/monitor/disable' in firmware
 else:assert route in firmware,route
for retired in ['stormStartUi','manualChooser','labGuided','pezWarmApply','chiAuroraApply','exportCurves']:
 assert not re.search(r'function\s+'+retired+r'\(',html)
assert 'function lampPresentation(' in html and 'function renderLampStates(' in html
assert 'diagLoad()' not in html and 'function refreshLiveDisplay()' in html
print('PASS all '+str(len(routes))+' browser API paths have firmware handlers; retired controls removed and shared state is the only presentation source')
