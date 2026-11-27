from pathlib import Path
s=Path('src/main.cpp').read_text()
setup=s[s.index('cielo96::restarts::append(restartJournal,entry);'):s.index('priorEvent=prefs.getString')]
assert 'trackedJournalWrite' not in setup and 'restartJournalDirty=true' in setup
assert 'v.latencyValid=false;' in s
assert all(f'stableLatency315[{i}].warning' in s for i in range(3))
assert 'nextPlan315!=bootNightPlan315' in s
assert 'night_probe_enabled\\\":false' in s
assert 'restart_exact_esp_error_available\\\":false' in s
print('PASS REV3 integration source contract (not target compilation)')
