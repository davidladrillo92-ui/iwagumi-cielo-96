"""Regression checks for the actual persistent-link safety wiring."""
from pathlib import Path
s=Path('src/main.cpp').read_text()
w=Path('src/cielo_ble_worker.cpp').read_text()
h=Path('include/cielo_ble_worker.h').read_text()
sdk=Path('vendor/bluedroid/BLEClient.cpp').read_text()
assert 'ble_keep_links(!otaBusy&&otaTxn.phase==OtaPhase::Idle' in s
assert 'if((!keepLinks[device].load() || !success)' in w
assert 'if(targets[k].client && targets[k].client->isConnected() && !keepLinks[k].load())' in w
assert 'unexpectedLoss[device].fetch_add(1)' in w
assert 'unexpected_disconnects' in h and 'sdk_connect_stage' in h
assert 'bleUnexpectedDisconnectTick();' in s
assert 'pvDisconnectInterlock=true;' in s
assert 'pvOffObligation=true;pvOffNextTry=millis();' in s
assert '&& !pvDisconnectInterlock' in s
assert 'pvOffNextTry=now+300000UL;' in s
assert 'cieloRequestReturn("Desconexion BLE inesperada:' in s
assert 'chReconnect.cancel();' in s
assert 'cieloConnectStage()' in w
assert 'm_cieloStage=2;' in sdk and 'm_cieloStage=4;' in sdk and 'm_cieloStage=5;' in sdk
assert 'if(!codec::ch_custom_curve_resume' in w
assert 'CH_AUTO_MODE_UNVERIFIED' in w
print('PASS persistent BLE and safe fallbacks: PV OFF, CH 0x03, SDK stage, OTA release')
