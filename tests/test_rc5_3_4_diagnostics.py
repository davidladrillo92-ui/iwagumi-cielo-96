"""Regression guards for non-destructive RECOVERY3.4 diagnostics.

These are static integration checks, not proof of physical BLE or flash behaviour.
"""
from pathlib import Path

source = Path("src/main.cpp").read_text()
plan = Path("docs/RECOVERY3_4_PLAN_CINCO_PUNTOS_20261026.md").read_text()

# NVS inspection must not destroy user configuration.
assert '#include <nvs.h>' in source
assert 'nvs_get_stats("nvs",&nvsStats)' in source
assert 'last_curve_bytes_requested' in source
assert 'last_curve_bytes_written' in source
assert 'curve_write_failures_this_boot' in source
assert 'PREFERENCES_PUTBYTES_RETURNS_SIZE_ONLY' in source
assert 'prefs.clear()' not in source
assert 'prefs.putBytes(key,&candidate,sizeof(candidate))' in source
assert 'memcmp(&candidate,&readback,sizeof(readback))!=0' in source

# Historical evidence is annotated, not rewritten.
assert 'FUTURE_UNTRUSTED' in source
assert 'following_boot_recovered' in source
assert 'restarts::at(restartJournal,i+1)' in source
assert 'restarts::recovered(restartJournal' in source

# Memory measurements are read-only and sampled, not persisted every tick.
assert 'largestHeapBlockMin15m' in source
assert 'largestHeapBlockSamples15m++' in source
assert 'heap_largest_block_min_15m_samples' in source

# All five scopes are captured before altering live BLE/safety policy.
for phrase in (
    'almacenamiento NVS', 'Recuperación de Chihiros',
    'Recuperación diurna de Pez Verde', 'Memoria mínima',
    'Historial de reinicios',
):
    assert phrase in plan, phrase

print("PASS RECOVERY3.4 non-destructive NVS, RAM and restart diagnostic guards")
