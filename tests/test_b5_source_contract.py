#!/usr/bin/env python3
"""Pruebas de regresión de invariantes verificables en el código fuente.
No afirman ejecución de BLE real ni prueban la serialización en hardware.
"""
from pathlib import Path
from html.parser import HTMLParser
r=Path(__file__).resolve().parents[1]
s=(r/'src/main.cpp').read_text()
w=(r/'src/cielo_ble_worker.cpp').read_text()
ui=(r/'web/interfaz_r4.html').read_text()
assert 'j+=",\\"persistent_diagnostics\\":{' in s
assert 'prefs.getBytesLength("boot_hist")' in s
assert 'prefs.putBytes("boot_hist",bootHistory' in s
assert 'prefs.getBytesLength("diag_journal")' in s
assert 'prefs.putBytes("diag_journal",&journal' in s
assert 'journalHasFlushed=true;journalLastFlush=millis()' in s
assert 'void flushJournal()' in s
assert 'static void persistCritical' in s and '(void)detail;' in s
assert 'srv.hasArg("share")?String("null"):jsonQuote(events[idx].detail)' in s
assert 'srv.hasArg("share")?String("null"):jsonQuote(priorEventDetail)' in s
assert 'j+=",\\"phase_timing_ms\\":{' in s
assert 'queue.pending(cielo::ble::Device::PezVerde)' in s
assert 'ble_request_queue_depth()' in s
assert 'pvOffLastGatt' in s and 'pvOffAttempts' in s
assert 'String(profile.pvCount)' in s and 'String(profile.chCount)' in s
assert 'metricTime(metrics.connect_last' in w
assert 'metricTime(metrics.write_last' in w
assert 'metricTime(metrics.disconnect_last' in w
assert 'id="serviceRefresh"' not in ui
assert 'id="diagShare"' in ui
assert 'diagReportFetchedAt' in ui
assert 'setInterval(()=>{if(state.view===\'lab\'||$(\'diagReportText\'))' in ui
class Buttons(HTMLParser):
 def __init__(self):super().__init__();self.ids=[]
 def handle_starttag(self,tag,attrs):
  for key,value in attrs:
   if key=='id' and value and value.startswith('diag'):self.ids.append(value)
p=Buttons();p.feed(ui)
assert len(p.ids)==len(set(p.ids)), 'Elementos de diagnóstico con IDs duplicados'
print('B5 source contracts: PASS')

assert 'resetReasonName((esp_reset_reason_t)b.cause)' in s
assert '(badReset&&!currentBootMarkedHealthy)' in s
assert 'CIELO_INFORME_COMPARTIBLE.json' in s
