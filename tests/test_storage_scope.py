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
          '  if(res.success && res.device<2 && bootFirstGattMs[res.device]==0)bootFirstGattMs[res.device]=now-bootMs;\n'
        )
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
for p,h in f['files'].items():assert hashlib.sha256(Path(p).read_bytes()).hexdigest()==h,p
fs=Path('include/cielo_profile_fs.h').read_text()
for mutation in ['SPIFFS.begin(true','SPIFFS.format(','esp_spiffs_format(','esp_spiffs_check(']:assert mutation not in fs
print('PASS storage scope: engine/BLE/recovery/partition and3.7 storage hashes unchanged; admission/UI changes authorized; no format/check/repair fallback')
