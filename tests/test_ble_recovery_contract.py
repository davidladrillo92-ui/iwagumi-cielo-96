from pathlib import Path
s=Path('src/main.cpp').read_text()
recovery=s[s.index('static void bleRecoveryTick()'):s.index('void setup()')]
assert 'armRestart' in recovery and 'ESP.restart()' in recovery
assert 'otaBusy || rescueOpen' in recovery
for phase in ('Preparing','Ready','Uploading','Completed'):assert 'OtaPhase::'+phase in recovery
assert 'prefs.clear(' not in recovery and 'vTaskDelete(' not in recovery and 'BLEDevice::deinit(' not in recovery
assert 'RTC_NOINIT_ATTR static cielo96::recovery::Retained' in s
assert 'esp_reset_reason()!=ESP_RST_SW' in s
assert 'if(bleRecoveryBootPvSafe)lightingTask();' in s
assert s.index('if(bleRecoveryBootPvSafe)lightingTask();')<s.index('if(bleReady && !rescueOpen){tracePhase(TracePhase::Ble);bleTick();bleUnexpectedDisconnectTick();}')
assert '!night && pvOffObligation && !bleRecoveryBootPvSafe' in s
assert 'if(!bleRecoveryBootPvSafe)return;' in s
assert 'bleRecoveryBootPvSafe && pvOffLastGatt && currentBootMarkedHealthy && timeOK()' in s
assert 'cieloAlwaysOn && !bleRecoveryBootPvSafe && currentBootMarkedHealthy' in s
print('PASS recovery integration: OTA/rescue inhibition, retained evidence and initial POWER OFF interlock')
