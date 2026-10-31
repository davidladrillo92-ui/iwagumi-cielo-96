#pragma once
// Read-only evidence helpers. No radio, filesystem or persistent writes.
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "cielo_curve_policy.h"
namespace cielo96 { namespace evidence {
inline uint32_t windowSamples(uint32_t total){return total<32?total:32;}
inline uint32_t basisPoints(uint32_t count,uint32_t total){
 if(!total)return 0;
 if(count>total)count=total;
 return uint32_t(10000ULL*count/total);
}
enum class Save : uint8_t { NotAttempted, Unchanged, Verified, Failed, Uncertain };
inline Save saveResult(bool attempted,bool unchanged,bool verified,bool failed,bool uncertain){
 if(unchanged)return Save::Unchanged;
 if(!attempted)return Save::NotAttempted;
 if(failed)return uncertain?Save::Uncertain:Save::Failed;
 return verified?Save::Verified:uncertain?Save::Uncertain:Save::Failed;
}
inline const char* saveName(Save s){switch(s){
 case Save::Unchanged:return "UNCHANGED";case Save::Verified:return "VERIFIED";
 case Save::Failed:return "FAILED";case Save::Uncertain:return "UNCERTAIN";
 default:return "NOT_ATTEMPTED_THIS_BOOT";
}}
// Same wire checksum as profile export/import, excluding generation and metadata.
inline uint32_t wireChecksum(const ProfileImage& p){
 uint32_t h=2166136261UL;char row[32];
 for(uint16_t i=0;i<p.pvCount&&i<64;i++){
  const auto& v=p.pv[i];int n=snprintf(row,sizeof(row),"%u,%u,%u,%u,%u",unsigned(v.minute),unsigned(v.r),unsigned(v.g),unsigned(v.b),unsigned(v.brightness));
  if(i)h=curves::fnvUpdate(h,";",1);
  h=curves::fnvUpdate(h,row,size_t(n));
 }
 h=curves::fnvUpdate(h,"|",1);
 for(uint16_t i=0;i<p.chCount&&i<64;i++){
  const auto& v=p.ch[i];int n=snprintf(row,sizeof(row),"%u,%u,%u,%u",unsigned(v.minute),unsigned(v.r),unsigned(v.g),unsigned(v.b));
  if(i)h=curves::fnvUpdate(h,";",1);
  h=curves::fnvUpdate(h,row,size_t(n));
 }return h;
}
// Whitelist only. Repeated OFF successes cannot evict maintenance evidence.
inline const char* eventName(uint8_t code){
 static const char* const names[]={"", "MANTENIMIENTO_COMPROBANDO", "MANTENIMIENTO_INICIO",
  "MANTENIMIENTO_LISTO", "MANTENIMIENTO_RETORNO", "MANTENIMIENTO_FIN",
  "MANTENIMIENTO_CANCELADO", "CURVA_GUARDADA_FS", "PERFIL_ATOMICO_GUARDADO",
  "CURVA_ERROR", "PV_DIA_REANUDADO", "BLE_LISTO", "OTA_VALIDADA"};
 return code<sizeof(names)/sizeof(names[0])?names[code]:"";
}
inline uint8_t eventCode(const char* name){
 if(!name)return 0;
 for(uint8_t i=1;i<=12;i++)if(strcmp(name,eventName(i))==0)return i;
 return 0;
}
struct Entry {uint32_t uptime_ms=0,epoch=0;uint8_t code=0;};
struct OperationalHistory {
 static constexpr uint8_t Capacity=16;
 Entry entries[Capacity]{};uint8_t head=0,count=0;
 void capture(const char* name,uint32_t uptime,uint32_t trustedEpoch){
  const uint8_t code=eventCode(name);if(!code)return;
  entries[head]={uptime,trustedEpoch,code};head=(head+1)%Capacity;
  if(count<Capacity)count++;
 }
 const Entry& at(uint8_t index)const{return entries[(head+Capacity-count+index)%Capacity];}
};
} }
