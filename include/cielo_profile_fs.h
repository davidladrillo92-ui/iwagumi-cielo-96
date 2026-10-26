#pragma once
// RECOVERY3.5: independent A/B curve snapshots in the EXISTING SPIFFS partition.
// Never repartition, erase NVS or format ANY partition, including blank flash.
#include <Arduino.h>
#include <SPIFFS.h>
#include <FS.h>
#include <esp_partition.h>
#include <string.h>
#include "cielo_curve_policy.h"
#include "cielo_nvs_safe.h"

namespace cielo96 { namespace profilefs {

static constexpr const char* kA="/cielo_profile_a.bin";
static constexpr const char* kB="/cielo_profile_b.bin";
static constexpr const char* kTemp="/cielo_profile_tmp.bin";

class Store {
 bool ready_=false;
 bool published_=false;
 enum class RawState:uint8_t {NotRead,Blank,Nonempty,ReadError};
 RawState rawState_=RawState::NotRead;
 esp_err_t rawError_=ESP_OK;
 size_t rawBytes_=0,firstData_=0;
 size_t partitionAddress_=0,partitionSize_=0;
 bool partitionEncrypted_=false;
 const char* status_="NOT_INITIALIZED";
 const char* error_="NONE";
 const char* slot_="NONE";

 void inspectRaw(const esp_partition_t* partition) {
  uint8_t bytes[1024];
  for(size_t off=0;off<partition->size;off+=sizeof(bytes)) {
   const size_t n=(partition->size-off)<sizeof(bytes)?partition->size-off:sizeof(bytes);
   rawError_=esp_partition_read(partition,off,bytes,n);
   if(rawError_!=ESP_OK){rawState_=RawState::ReadError;return;}
   rawBytes_+=n;
   for(size_t i=0;i<n;i++)if(bytes[i]!=0xff){rawState_=RawState::Nonempty;firstData_=off+i;return;}
   if((off%16384)==0)delay(1);
  }
  rawState_=RawState::Blank;
 }
public:
 bool begin() {
  ready_=false;slot_="NONE";error_="NONE";published_=false;
  rawState_=RawState::NotRead;rawError_=ESP_OK;rawBytes_=0;firstData_=0;
  partitionAddress_=0;partitionSize_=0;partitionEncrypted_=false;
  // This layout was inspected from the original ATOMS3 Lite flash. Refuse
  // any different partition instead of touching an unknown filesystem.
  const esp_partition_t* part=esp_partition_find_first(
    ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,"spiffs");
  if(part){partitionAddress_=part->address;partitionSize_=part->size;partitionEncrypted_=part->encrypted;}
  if(!part||part->address!=0x670000UL||part->size!=0x180000UL){
   status_="PARTITION_MISMATCH_OR_MISSING";return false;
  }
  if(partitionEncrypted_){status_="ENCRYPTED_PARTITION_UNSUPPORTED_PRESERVED";return false;}
  if(SPIFFS.begin(false,"/spiffs",4,"spiffs")){
   ready_=true;status_="MOUNTED_EXISTING";return true;
  }
  // Failure does not prove corruption or even nonempty flash. Read errors
  // must not masquerade as existing content. No format/check/repair fallback.
  inspectRaw(part);
  status_=rawState_==RawState::ReadError?"PARTITION_READ_FAILED_PRESERVED":
   rawState_==RawState::Blank?"UNMOUNTABLE_BLANK_PRESERVED":"UNMOUNTABLE_NONEMPTY_PRESERVED";
  return false;
 }
 bool ready() const {return ready_;}
 bool initializedFromBlank() const {return false;} // compatibility: automatic format removed
 const char* rawState() const {
  switch(rawState_){case RawState::Blank:return "ALL_FF";case RawState::Nonempty:return "NON_FF_FOUND";
   case RawState::ReadError:return "READ_ERROR";default:return "NOT_READ";}
 }
 int rawError() const {return rawError_;}
 size_t rawBytesRead() const {return rawBytes_;}
 bool hasFirstData() const {return rawState_==RawState::Nonempty;}
 size_t firstDataOffset() const {return firstData_;}
 size_t partitionAddress() const {return partitionAddress_;}
 size_t partitionSize() const {return partitionSize_;}
 bool partitionEncrypted() const {return partitionEncrypted_;}
 const char* status() const {return status_;}
 const char* error() const {return error_;}
 const char* slot() const {return slot_;}
 bool published() const {return published_;}
 size_t total() const {return ready_?SPIFFS.totalBytes():0;}
 size_t used() const {return ready_?SPIFFS.usedBytes():0;}
 bool read(const char* path,ProfileImage& out) const {
  if(!ready_||!SPIFFS.exists(path))return false;
  File f=SPIFFS.open(path,"r");
  if(!f)return false;
  if(f.size()!=sizeof(out)){f.close();return false;}
  const size_t n=f.read(reinterpret_cast<uint8_t*>(&out),sizeof(out));
  f.close();
  return n==sizeof(out) && curves::curveValid(out);
 }
 bool newest(ProfileImage& out) const {
  ProfileImage a{},b{};
  const bool va=read(kA,a),vb=read(kB,b);
  const ProfileImage* newer=curves::newestValid(va?&a:nullptr,vb?&b:nullptr);
  if(!newer)return false;
  out=*newer;return true;
 }
 bool save(const ProfileImage& candidate) {
  error_="NONE";slot_="NONE";published_=false;
  if(!ready_){error_="NOT_MOUNTED";return false;}
  if(!curves::curveValid(candidate)){error_="INVALID_PROFILE";return false;}
  ProfileImage a{},b{},readback{};
  const bool existsA=SPIFFS.exists(kA),existsB=SPIFFS.exists(kB);
  const bool va=read(kA,a),vb=read(kB,b);
  // An existing unreadable file is not an absent slot. Never erase a
  // potentially active snapshot after a transient I/O/CRC/read failure.
  if((existsA&&!va)||(existsB&&!vb)){error_="EXISTING_SLOT_UNREADABLE";return false;}
  const ProfileImage* active=curves::newestValid(va?&a:nullptr,vb?&b:nullptr);
  if(active && static_cast<int32_t>(candidate.generation-active->generation)<=0){
   error_="CANDIDATE_NOT_NEWER";return false;
  }
  const char* activePath=active==&a?kA:kB;
  const char* legacy=nvssafe::target(va,va?a.generation:0,vb,vb?b.generation:0);
  const char* destination=strcmp(legacy,"curvesA")==0?kA:kB;
  slot_=destination;
  if((va&&!vb&&destination!=kB)||(!va&&vb&&destination!=kA)){
   error_="ACTIVE_SLOT_PROTECTED";return false;
  }
  // Prepare a complete independent snapshot first. The current active
  // profile is never modified until this tempfile has been verified.
  if(SPIFFS.exists(kTemp)&&!SPIFFS.remove(kTemp)){
   error_="OLD_TEMP_REMOVE_FAILED";return false;
  }
  File f=SPIFFS.open(kTemp,"w");
  if(!f){error_="TEMP_OPEN_FAILED";return false;}
  const size_t bytes=f.write(reinterpret_cast<const uint8_t*>(&candidate),sizeof(candidate));
  f.flush();f.close();
  if(bytes!=sizeof(candidate)){
   error_="TEMP_SHORT_WRITE";SPIFFS.remove(kTemp);return false;
  }
  if(!read(kTemp,readback)||memcmp(&readback,&candidate,sizeof(candidate))!=0){
   error_="TEMP_VERIFY_FAILED";SPIFFS.remove(kTemp);return false;
  }
  // Check the active bytes immediately before removing the inactive file.
  if(active){
   readback=ProfileImage{};
   if(!read(activePath,readback)||memcmp(&readback,active,sizeof(readback))!=0){
    error_="ACTIVE_RECHECK_FAILED";return false;
   }
  }
  // Remove ONLY the inactive or older destination. The latest FS copy
  // (or the preserved legacy NVS copy for the first migration) survives.
  if(SPIFFS.exists(destination)&&!SPIFFS.remove(destination)){
   error_="OLD_SLOT_REMOVE_FAILED";return false;
  }
  if(!SPIFFS.rename(kTemp,destination)){
   error_="RENAME_FAILED";return false;
  }
  published_=true;
  readback=ProfileImage{};
  if(!read(destination,readback)||memcmp(&readback,&candidate,sizeof(candidate))!=0){
   error_="FINAL_VERIFY_FAILED";return false;
  }
  error_="NONE";return true;
 }
};

inline bool newerGeneration(uint32_t candidate,uint32_t current){
 return static_cast<int32_t>(candidate-current)>0;
}
}} // namespace cielo96::profilefs
