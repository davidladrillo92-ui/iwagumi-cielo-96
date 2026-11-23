"""RECOVERY3.9 audits UI/status only; freeze3.8 maintenance plus engine, BLE and3.7 storage."""
from pathlib import Path
import json,hashlib
f=json.loads(Path('tests/fixtures/maintenance_3_9_frozen_sha256.json').read_text())
s=Path('src/main.cpp').read_text()
for r in f['ranges']:
    part=s[s.index(r['start']):s.index(r['end'],s.index(r['start']))]
    if r['name']=='disconnect':
        # 3.13 changes exactly the ownership retry on unexpected CH loss.
        new='cieloChRun=false;\n   if(cieloAlwaysOn)chReconnect.arm(millis());else chReconnect.cancel();'
        assert part.count(new)==1
        part=part.replace(new,'cieloChRun=false;chReconnect.cancel();')
    if r['name']=='lighting':
        assert part.count('if(!clockEvidenceTrusted() || !cielo96::profileValid(profile))')==1
        part=part.replace('if(!clockEvidenceTrusted() || !cielo96::profileValid(profile))',
                          'if(!timeOK() || !cielo96::profileValid(profile))')
    if r['name']=='ble':
        # 3.14 explicitly permits only these measured BLE spans, a new
        # boot timestamp, scan classification, boot-day priority and IDs.
        added=(
          '  lastBleSpan[device]=res;\n'
          '  if(cielo96::r314::cold_discovery_slow(res.duration_ms,res.discovery_ms))++bleColdDiscoverySlow[device];\n'
          '  else if(res.duration_ms>3500UL)++bleWarmSlow[device];\n'
        )
        first_gatt='  if(res.success && res.device<2 && bootFirstGattMs[res.device]==0)bootFirstGattMs[res.device]=now-bootMs;\n'
        assert part.count(first_gatt)==1
        part=part.replace(first_gatt,'')
        assert part.count(added)==1
        part=part.replace(added,'')
        slow_new=('   if(res.duration_ms>bleDurationMax)bleDurationMax=res.duration_ms;\n'
          '  if(res.duration_ms>3500UL){bleSlowCount++;\n'
          '    const bool cold=cielo96::r314::cold_discovery_slow(res.duration_ms,res.discovery_ms);\n'
          '    record(cold?"BLE_LENTO_DESCUBRIMIENTO":"BLE_LENTO",String(res.device?"CH ":"PV ")+String(res.duration_ms)+"ms; scan="+String(res.discovery_ms)+"ms");}')
        slow_old=('  if(res.duration_ms>bleDurationMax)bleDurationMax=res.duration_ms;\n'
          '  if(res.duration_ms>3500UL){bleSlowCount++;record("BLE_LENTO",String(res.device?"CH ":"PV ")+String(res.duration_ms)+"ms");}')
        assert part.count(slow_new)==1
        part=part.replace(slow_new,slow_old)
        priority_new=('const bool pvBootDayWait=cielo96::r314::prioritize_pv_day(now-bootMs,\n'
          '   bleRecoveryBootPvSafe,pvOffLastGatt!=0,validBootDay,clockEvidenceTrusted());')
        priority_old=('const bool pvBootDayWait=cielo96::pvday::prioritize_boot_day(bleRecoveryBoot,\n'
          '   bleRecoveryBootPvSafe,pvOffLastGatt!=0,validBootDay,otaCandidateBoot,now-bootMs);')
        assert part.count(priority_new)==1
        part=part.replace(priority_new,priority_old)
        request_new=('   const uint32_t requestId=nextCustomResumeId();\n'
          '   if(cielo96::ble_custom_resume_request(requestId)){\n'
          '    customExpectedRequestId=requestId;')
        assert part.count(request_new)==1
        part=part.replace(request_new,'   if(cielo96::ble_custom_resume_request()){')
        assert part.count('const bool matched=queue.complete(res.sequence,res.success,now);')==1
        part=part.replace('const bool matched=queue.complete(res.sequence,res.success,now);','queue.complete(res.sequence,res.success,now);')
        new='\n     // A late completion from the previous owner must not cancel the armed retry.\n     if(matched&&cieloChRun&&!cieloChReturnPending&&!cieloChAwaitResponse)chReconnect.success();'
        assert part.count(new)==1
        part=part.replace(new,'chReconnect.success();')
    assert hashlib.sha256(part.encode()).hexdigest()==r['sha256'],r['name']
for p,h in f['files'].items():
    raw=Path(p).read_bytes()
    if p=='src/cielo_ble_worker.cpp':
        worker=raw.decode()
        fast=' BLEScanResults result=scan->start(2,false);'
        assert worker.count(fast)==1, 'BLE discovery fast-first marker missing'
        worker=worker.replace(fast, ' BLEScanResults result=scan->start(5,false);')
        marker=' // A short first window saves 3 s for nearby lamps.'
        assert worker.count(marker)==1, 'BLE 5s fallback removed unexpectedly'
        at=worker.index(marker)
        end=worker.index(' return found;',at)
        assert 'BLEScanResults fallback=scan->start(5,false);' in worker[at:end]
        worker=worker[:at]+worker[end:]
        replacements=[
          ('// Written exclusively by the BLE worker during one command.\nstruct OpSpan {uint32_t discovery=0,connect=0,service=0,characteristic=0,write=0,pacing=0;};\nOpSpan opSpan{};\n',''),
          (' const uint32_t scanStart=millis();\n',''),
          (' opSpan.discovery+=uint32_t(millis()-scanStart);\n',''),
          (' const uint32_t connectDuration=millis()-started;\n opSpan.connect+=connectDuration;\n metricTime(metrics.connect_last,metrics.connect_max,connectDuration);',
           ' metricTime(metrics.connect_last,metrics.connect_max,millis()-started);'),
          (' const uint32_t serviceStart=millis();\n',''),
          (' opSpan.service+=uint32_t(millis()-serviceStart);\n',''),
          (' const uint32_t characteristicStart=millis();\n',''),
          (' opSpan.characteristic+=uint32_t(millis()-characteristicStart);\n',''),
          (' const uint32_t writeDuration=millis()-writeStarted;\n opSpan.write+=writeDuration;\n metricTime(metrics.write_last,metrics.write_max,writeDuration);',
           ' metricTime(metrics.write_last,metrics.write_max,millis()-writeStarted);'),
          (' const uint32_t paceStart=millis();\n',''),
          (' opSpan.pacing+=uint32_t(millis()-paceStart);\n',''),
          ('void resumeCustomCurve(uint32_t requestId){\n BleCustomResumeResult res{};res.request_id=requestId;uint32_t started=millis();\n opSpan=OpSpan{};const char*err="OK";',
           'void resumeCustomCurve(){\n BleCustomResumeResult res{};uint32_t started=millis();const char*err="OK";'),
          ('  uint32_t customToken=0;', '  uint8_t customToken=0;'),
          ('   resumeCustomCurve(customToken);touchHeartbeat();','   resumeCustomCurve();touchHeartbeat();'),
          ('offWrittenInCurrentOperation=false;opSpan=OpSpan{};const char*err="OK";',
           'offWrittenInCurrentOperation=false;const char*err="OK";'),
          ('  res.off_sent=offWrittenInCurrentOperation;res.success=success;res.duration_ms=millis()-started;res.write_calls=writesInCurrentOperation;\n  res.discovery_ms=opSpan.discovery;res.connect_ms=opSpan.connect;\n  res.service_ms=opSpan.service;res.characteristic_ms=opSpan.characteristic;\n  res.write_ms=opSpan.write;res.pacing_ms=opSpan.pacing;\n  copyReason(res,err);',
           '  res.off_sent=offWrittenInCurrentOperation;res.success=success;res.duration_ms=millis()-started;res.write_calls=writesInCurrentOperation;copyReason(res,err);'),
          ('sizeof(BleProbeResult),sizeof(uint8_t),sizeof(uint32_t),sizeof(BleCustomResumeResult)',
           'sizeof(BleProbeResult),sizeof(uint8_t),sizeof(uint8_t),sizeof(BleCustomResumeResult)'),
          ('bool ble_custom_resume_request(uint32_t request_id){\n return request_id!=0 && customRequests && xQueueSend(customRequests,&request_id,0)==pdTRUE;\n}',
           'bool ble_custom_resume_request(){\n uint8_t token=1;\n return customRequests && xQueueSend(customRequests,&token,0)==pdTRUE;\n}')
        ]
        for added,original in replacements:
            assert worker.count(added)==1,('BLE 3.14 unexpected diff',added)
            worker=worker.replace(added,original)
        raw=worker.encode()
    assert hashlib.sha256(raw).hexdigest()==h,p
fs=Path('include/cielo_profile_fs.h').read_text()
for mutation in ['SPIFFS.begin(true','SPIFFS.format(','esp_spiffs_format(','esp_spiffs_check(']:assert mutation not in fs
print('PASS storage scope: engine/BLE/recovery/partition and3.7 storage hashes unchanged; admission/UI changes authorized; no format/check/repair fallback')
