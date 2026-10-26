#pragma once
#include "cielo_profile_fs.h"
#include "cielo_littlefs_store.h"
namespace cielo96 { namespace storage {
// Existing SPIFFS compatibility and signature-gated LittleFS backend.
class CurveStore {
 profilefs::Store spiffs_;
 LittlefsReadonlyProbe& probe_;
 bool isLittle_=false;
 bool blocked_=false;
 const char* blockedReason_="NONE";
public:
 littlecurve::Store little;
 explicit CurveStore(LittlefsReadonlyProbe&probe):probe_(probe){}
 bool begin(){
  if(spiffs_.begin())return true;
  probe_.run();
  isLittle_=probe_.verified;
  return isLittle_&&little.begin(probe_);
 }
 bool ready()const{return isLittle_?little.ready():spiffs_.ready();}
 bool isLittlefs()const{return isLittle_;}
 const char* backend()const{return ready()?(isLittle_?"LITTLEFS_AB":"SPIFFS_AB"):"NVS_LEGACY";}
 bool allowNvsWrites()const{return !blocked_&&!probe_.signature0&&!probe_.signature1&&!isLittle_&&probe_.readError==ESP_OK&&!probe_.mountAttempted;}
 bool writable()const{return ready()&&!blocked_&&(!isLittle_||!little.blocked());}
 bool newest(ProfileImage&out){return isLittle_?little.newest(out):spiffs_.newest(out);}
 bool save(const ProfileImage&p){return !blocked_&&(isLittle_?little.save(p):spiffs_.save(p));}
 bool migrate(const ProfileImage&p){return isLittle_&&little.migrate(p);}
 void block(const char*reason){blocked_=true;blockedReason_=reason;little.block(reason);}
 const char* status()const{return blocked_?blockedReason_:(isLittle_?little.status():spiffs_.status());}
 const char* error()const{return blocked_?blockedReason_:(isLittle_?little.error():spiffs_.error());}
 const char* slot()const{return isLittle_?little.slot():spiffs_.slot();}
 bool published()const{return isLittle_?little.published():spiffs_.published();}
 bool uncertain()const{return isLittle_?little.uncertain():spiffs_.published();}
 bool initializedFromBlank()const{return false;}
 size_t total(){return isLittle_?little.total():spiffs_.total();}
 size_t used(){return isLittle_?little.used():spiffs_.used();}
 const char* rawState()const{return spiffs_.rawState();}
 int rawError()const{return spiffs_.rawError();}
 size_t rawBytesRead()const{return spiffs_.rawBytesRead();}
 bool hasFirstData()const{return spiffs_.hasFirstData();}
 size_t firstDataOffset()const{return spiffs_.firstDataOffset();}
 size_t partitionAddress()const{return spiffs_.partitionAddress();}
 size_t partitionSize()const{return spiffs_.partitionSize();}
 bool partitionEncrypted()const{return spiffs_.partitionEncrypted();}
};
}}
