"""LAN1 explicitly replaces the earlier password/TLS requirement at owner's request."""
from pathlib import Path
src=Path('src/main.cpp').read_text();ui=Path('web/interfaz_r4.html').read_text()
assert '11.1.0-RC6-RECOVERY2' in src
for old in ('authSessionOpen','verifyFirmwareSignature','https_begin(prefs)','sessionValid()'):
 assert old not in src,old
for old in ('id="otaSecret"','id="otaSignatureFile"','cieloEnsureSession','X-Cielo-Session','X-Cielo-Key'):
 assert old not in ui,old
assert 'sameLanOrigin' in src and 'safeFetchSite' in src
assert 'otaTxn.phase==OtaPhase::Ready && requireAdminWrite()' in src
assert 'otaNetworkReady() && currentBootMarkedHealthy' in src
assert src.index('compatiblePrefix(otaPrefix')<src.index('Update.end(true)')
assert src.index('prefs.putString("ota_prev",otaRunningPartition)')<src.index('Update.end(true)')
assert 'otaBytes<=target->size-upload.currentSize' in src
assert 'otaBusy && millis()-otaLastChunkAt>120000UL' in src
assert 'UPLOAD_FILE_ABORTED' in src and 'otaAbortPreparation' in src
assert 'boot_observation_remaining_s' in src
assert 'OTA_ROLLBACK' in src and '600000UL' in src
assert 'X-Cielo-Sha256' in src and 'X-Cielo-Sha256' in ui
assert 'if(rescueOpen){fail(403' in src
assert 'physical_required\\":false' in src
print('PASS LAN: no login/TLS/signature, origin guard, OTA preparation/hash/compatibility/rollback retained')
