#!/usr/bin/env python3
"""CIELO RC5 B3: static safety contracts; real BLE/TLS requires hardware."""
from pathlib import Path
src=Path('src/main.cpp').read_text()
https=Path('src/cielo_https_proxy.cpp').read_text()
ui=Path('web/interfaz_r4.html').read_text()
workflow=Path('.github/workflows/cielo-11-1-build.yml').read_text()
assert '11.1.0-RC5-B1B5B3-B2B4-CURVAS' in src
assert '11.1.0-RC5-B1B5B3-B2B4-CURVAS' in Path('platformio.ini').read_text()
# Session only RAM (no persistent token or password URL): login needs TLS.
assert 'static uint8_t adminSession[32]' in src
assert 'esp_fill_random(adminSession,sizeof(adminSession))' in src
assert 'X-Cielo-Session' in src and 'X-Cielo-Session' in https
assert 'authSessionOpen' in src and 'authSessionClose' in src
assert 'return constantTime32(given,stored)' in src
assert 'if(!sessionValid())' in src and 'SESSION_IDLE_MS=15UL*60UL*1000UL' in src
assert 'prefs.putString("session"' not in src and 'localStorage.setItem(\'cieloSession' not in ui
# Password only for HTTPS login or first physical pairing; ordinary writes use RAM session.
assert 'async function cieloEnsureSession' in ui
assert "opt.headers['X-Cielo-Session']=await cieloEnsureSession()" in ui
assert "x.setRequestHeader('X-Cielo-Session',cieloSessionToken)" in ui
assert "x.setRequestHeader('X-Cielo-Key',key)" not in ui
assert 'authNextAttempt=now+1000UL' in src and 'authLockUntil=now+120000UL' in src
# Port 80 must NOT expose passwords, private reports, SSID and browser write controls.
for mark in ('srv.on("/api/status",HTTP_GET,[]{if(requireHttpsRead())statusJson();});',
             'srv.on("/api/diagnostico/maestro",HTTP_GET,[]{if(!requireHttpsRead())return;',
             'srv.on("/api/wifi/networks",HTTP_GET,[]{if(!requirePrivateRead())return;'):
    assert mark in src, mark
assert 'sameOrigin(origin.c_str(),host.c_str())' in https
assert 'safeFetchSite(site.c_str())' in https
assert 'X-Frame-Options' in https and 'Referrer-Policy' in https
assert 'Nombre Wi-Fi contiene caracteres de control' in src
assert 'srv.hasArg("share")&&!requirePrivateRead()' in src
assert 'srv.on("/api/wifi/networks",HTTP_GET,[]{if(!requirePrivateRead())return;' in src
assert "?share=1" in ui
assert 'cieloEscapeHtml(d.homeSsid' in ui
# OTA: physical gate + BLE preflight + cryptographic signature BEFORE boot partition flips.
assert 'physicalOtaGrant() && currentBootMarkedHealthy' in src
assert 'otaTxn.phase==OtaPhase::Ready && sessionValid()' in src
assert 'verifyFirmwareSignature(otaDigestOut' in src
assert src.index('verifyFirmwareSignature(otaDigestOut')<src.index('Update.end(true)')
assert src.index('prefs.putString("ota_prev",otaRunningPartition)')<src.index('Update.end(true)')
assert 'otaBytes+upload.currentSize>target->size' in src
assert 'otaBusy && millis()-otaLastChunkAt>120000UL' in src
assert 'UPLOAD_FILE_ABORTED' in src
assert 'boot_observation_remaining_s' in src
assert 'CIELO: conecta por HTTPS' in src
assert 'CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE' not in Path('platformio.ini').read_text()
assert 'dev/cielo-11-1-rc5-b1-b5-b3-ota-security-20261021' in workflow
# Regression: HTTPS Safari is the MAIN interface, not a preview.
assert "location.protocol==='https:'||location.protocol==='http:'" in ui
assert "await cieloEnsureSession()" in ui
assert "cieloSessionPending" in ui
assert 'srv.on("/api/ota/preflight",HTTP_GET,[]{if(requirePrivateRead())otaPreflight();});' in src
assert 'srv.on("/api/ota/prepare/status",HTTP_GET,[]{if(requirePrivateRead())otaPrepareStatus();});' in src
assert "fetch('/api/ota/preflight',{cache:'no-store',headers:{'X-Cielo-Session':await cieloEnsureSession()}})" in ui
assert "fetch('/api/ota/prepare/status',{cache:'no-store',headers:{'X-Cielo-Session':cieloSessionToken}})" in ui

# No changes to the colour interpolation engine or confirmed V4 source during B3.
assert Path('reference/CURVAS_NATURALES_V4_CONFIRMADAS_EN_ATOM.json').exists()
assert Path('include/cielo_engine.h').exists()
print('OK B3: TLS origin, short-lived sessions, protected read/write routes, OTA signature, physical grant and recovery guards')
