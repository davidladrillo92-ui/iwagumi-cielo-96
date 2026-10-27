#pragma once
// Complete Wi-Fi credentials live in one checksummed record. No Arduino, radio or IO here.
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace cielo96 { namespace wificfg {
constexpr uint32_t Magic=0x31464957UL; // WIF1, native ESP32 little-endian
struct Image {
 uint32_t magic=Magic,generation=0,checksum=0;
 uint8_t schema=1,enabled=0,ssidLength=0,passwordLength=0;
 uint8_t ssid[32]{},password[64]{};
};
static_assert(sizeof(Image)==112,"Wi-Fi record layout changed");
inline uint32_t hash(const Image& image){
 const auto* p=reinterpret_cast<const uint8_t*>(&image);uint32_t h=2166136261UL;
 for(size_t i=0;i<sizeof(image);i++)if(i<8||i>=12)h=(h^p[i])*16777619UL;
 return h;
}
inline void seal(Image& i){i.checksum=hash(i);}
inline bool payloadValid(const Image& i){
 if(i.enabled>1||i.ssidLength>32||i.passwordLength>63)return false;
 if(i.enabled && (!i.ssidLength||(i.passwordLength>0&&i.passwordLength<8)))return false;
 if(!i.enabled && (i.ssidLength||i.passwordLength))return false;
 for(size_t n=0;n<32;n++){
  if(n<i.ssidLength){if(i.ssid[n]<32||i.ssid[n]==127)return false;}
  else if(i.ssid[n])return false;
 }
 for(size_t n=0;n<64;n++){
  if(n<i.passwordLength){if(i.password[n]==0)return false;}
  else if(i.password[n])return false;
 }
 return true;
}
inline bool valid(const Image& i){return i.magic==Magic&&i.schema==1&&payloadValid(i)&&i.checksum==hash(i);}
inline bool make(Image& out,const char* ssid,size_t ssidSize,const char* pass,size_t passSize,bool enabled){
 out=Image{};out.enabled=enabled?1:0;
 if(ssidSize>32||passSize>63||(ssidSize&&!ssid)||(passSize&&!pass))return false;
 out.ssidLength=uint8_t(ssidSize);out.passwordLength=uint8_t(passSize);
 if(ssidSize)memcpy(out.ssid,ssid,ssidSize);
 if(passSize)memcpy(out.password,pass,passSize);
 seal(out);return payloadValid(out);
}
inline bool samePayload(const Image&a,const Image&b){
 return a.enabled==b.enabled&&a.ssidLength==b.ssidLength&&a.passwordLength==b.passwordLength&&
  memcmp(a.ssid,b.ssid,sizeof(a.ssid))==0&&memcmp(a.password,b.password,sizeof(a.password))==0;
}
inline const char* key(int slot){return slot==0?"wifiA":slot==1?"wifiB":nullptr;}
enum class Origin:uint8_t{Legacy,Atomic,Blocked};
struct Selection {
 Origin origin=Origin::Legacy;Image image{};int active=-1,error=0;
 bool degraded=false;
 const char* stage="LEGACY_READ_ONLY";
};
// Missing is different from read errors: errors must not resurrect legacy credentials.
template<class Store> Selection inspect(Store& store){
 Selection r;Image a{},b{};
 const int ea=store.read(key(0),a),eb=store.read(key(1),b);
 if((ea!=Store::Ok&&ea!=Store::Missing&&ea!=Store::Corrupt)||
    (eb!=Store::Ok&&eb!=Store::Missing&&eb!=Store::Corrupt)){
  r.origin=Origin::Blocked;r.stage="READ_FAILED";r.error=ea!=Store::Ok&&ea!=Store::Missing&&ea!=Store::Corrupt?ea:eb;return r;
 }
 if((ea==Store::Ok&&(a.magic!=Magic||a.schema!=1))||(eb==Store::Ok&&(b.magic!=Magic||b.schema!=1))){
  r.origin=Origin::Blocked;r.stage="UNKNOWN_SCHEMA";return r;
 }
 const bool va=ea==Store::Ok&&valid(a),vb=eb==Store::Ok&&valid(b);
 if(!va&&!vb){
  if(ea!=Store::Missing||eb!=Store::Missing){r.origin=Origin::Blocked;r.stage="NO_VALID_COPY";}
  return r;
 }
 int winner=va?0:1;
 if(va&&vb){
  const uint32_t diff=a.generation-b.generation;
  if((diff==0&&memcmp(&a,&b,sizeof(a)))||diff==0x80000000UL){r.origin=Origin::Blocked;r.stage="GENERATION_CONFLICT";return r;}
  winner=diff==0||diff<0x80000000UL?0:1;
 }
 r.origin=Origin::Atomic;r.active=winner;r.image=winner==0?a:b;
 r.degraded=!va||!vb;r.stage=r.image.enabled?"ATOMIC_VERIFIED":"DISABLED_VERIFIED";return r;
}
struct Result {
 bool verified=false,unchanged=false,mayHavePersisted=false,degraded=true;
 int error=0,slot=-1;uint32_t generation=0;
 const char* stage="VALIDATION";
};
// One caller (HTTP/main loop). Only the inactive Wi-Fi key is written; never erase/reclaim.
template<class Store> Result save(Store& store,const Image& desired,Image& verified){
 Result r;
 if(!valid(desired)){r.error=Store::Corrupt;return r;}
 const Selection current=inspect(store);
 if(current.origin==Origin::Blocked){r.error=current.error;r.stage=current.stage;return r;}
 auto survivorValid=[&](){
  if(current.origin!=Origin::Atomic)return true;
  Image check{};
  return store.read(key(current.active),check)==Store::Ok&&valid(check)&&memcmp(&check,&current.image,sizeof(check))==0;
 };
 if(!survivorValid()){r.stage="ACTIVE_CHANGED";return r;}
 if(current.origin==Origin::Atomic&&samePayload(current.image,desired)){
  verified=current.image;r.verified=true;r.unchanged=true;r.slot=current.active;r.generation=verified.generation;r.stage="UNCHANGED_VERIFIED";r.degraded=current.degraded;return r;
 }
 Image candidate=desired;candidate.generation=current.origin==Origin::Atomic?current.image.generation+1:1;seal(candidate);
 r.slot=current.active==0?1:0;r.generation=candidate.generation;
 r.stage="WRITE";
 int error=store.write(key(r.slot),candidate);r.mayHavePersisted=error!=Store::NoSpace;
 if(error!=Store::Ok){r.error=error;return r;}
 r.stage="COMMIT";if((error=store.commit())!=Store::Ok){r.error=error;return r;}
 // Fresh independent read, not a successful byte count or a shared-handle cache.
 r.stage="VERIFY";Image check{};
 if((error=store.read(key(r.slot),check))!=Store::Ok){r.error=error;return r;}
 if(!valid(check)||memcmp(&candidate,&check,sizeof(check))){r.error=Store::Corrupt;return r;}
 if(!survivorValid()){r.stage="ACTIVE_CHANGED";return r;}
 verified=check;r.verified=true;r.mayHavePersisted=false;r.stage="ATOMIC_VERIFIED";r.degraded=current.origin!=Origin::Atomic;return r;
}
} }
