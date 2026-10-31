// Included once, after statusJson; all values are read-only session evidence.
static String diagnosticEvidenceJson(){
 const bool fs=curveFs.ready();
 const bool failed=lastCurveNvsErr!=ESP_OK;
 const bool verified=fs?strcmp(lastCurveNvsStage,"FS_VERIFICADO")==0:lastCurveTransaction.verified;
 const bool uncertain=fs?(curveFs.uncertain()||(failed&&curveFs.published())):(lastCurveTransaction.writeMayHavePersisted&&!lastCurveTransaction.verified);
 const auto result=cielo96::evidence::saveResult(lastCurveNvsRequested>0,lastCurveRequestUnchanged,verified,failed,uncertain);
 String j;j.reserve(1600);
 j="{\"schema_version\":1,\"boot_id\":"+String(previousBootCount+1);
 j+=",\"scope\":\"CURRENT_BOOT\",\"optically_verified\":false";
 j+=",\"profile\":{\"generation\":"+String(profile.generation)+",\"backend\":"+jsonQuote(loadedCurveBackend);
 j+=",\"content_valid\":"+String(cielo96::curves::curveValid(profile)?"true":"false")+",\"wire_checksum\":"+jsonQuote(profileWireChecksum())+",\"algorithm\":\"fnv1a32\"}";
 j+=",\"curve_save\":{\"backend\":"+jsonQuote(fs?curveFs.backend():"NVS_LEGACY")+",\"last_result\":"+jsonQuote(cielo96::evidence::saveName(result));
 j+=",\"verified\":"+(result==cielo96::evidence::Save::NotAttempted?String("null"):String(result==cielo96::evidence::Save::Verified||result==cielo96::evidence::Save::Unchanged?"true":"false"));
 j+=",\"unchanged\":"+String(lastCurveRequestUnchanged?"true":"false")+",\"uncertain\":"+String(result==cielo96::evidence::Save::Uncertain?"true":"false");
 j+=",\"error_code\":"+(result==cielo96::evidence::Save::NotAttempted?String("null"):String(result==cielo96::evidence::Save::Unchanged?0:(int)lastCurveNvsErr))+",\"failure_count_this_boot\":"+String(curveNvsFailedAttempts)+"}";
 j+=",\"cadence\":{\"busy_checkpoints\":"+String(cadenceSkippedBusy)+",\"due_checkpoints\":"+String(cadenceDiagnostics.due);
 j+=",\"busy_basis_points\":"+(cadenceDiagnostics.due?String(cielo96::evidence::basisPoints(cadenceSkippedBusy,cadenceDiagnostics.due)):String("null"));
 j+=",\"evaluated_checkpoints\":"+String(cadenceDiagnostics.evaluated)+",\"inhibited_checkpoints\":"+String(cadenceDiagnostics.inhibited);
 j+=",\"missed_updates_measured\":false,\"missed_updates\":null}";
 j+=",\"operational_history\":{\"scope\":\"VOLATILE_CURRENT_BOOT\",\"capacity\":16,\"samples\":"+String(operationalHistory.count)+",\"events\":[";
 for(uint8_t i=0;i<operationalHistory.count;i++){
  const auto& e=operationalHistory.at(i);if(i)j+=",";
  j+="{\"uptime_ms\":"+String(e.uptime_ms)+",\"epoch\":"+(e.epoch?String(e.epoch):String("null"));
  j+=",\"event\":"+jsonQuote(cielo96::evidence::eventName(e.code))+"}";
 }
 return j+"]}}";
}
