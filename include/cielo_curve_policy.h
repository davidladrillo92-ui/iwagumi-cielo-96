#pragma once
// Politica de integridad del perfil CIELO V2: host C++17 y ESP32 sin heap.
#include <stddef.h>
#include <stdint.h>
#include "cielo_profile_store.h"
namespace cielo96 { namespace curves {
constexpr size_t MaxPoints=64;
constexpr uint16_t SchemaVersion=2;
inline bool curveValid(const ProfileImage& p){
 if(!profileValid(p) || p.pvCount<2 || p.chCount<2)return false;
 const auto &first=p.pv[0],&pv=p.pv[p.pvCount-1];
 const auto &ch=p.ch[p.chCount-1];
 return first.brightness==0 && pv.minute==1440 && pv.brightness==0 &&
   ch.minute==1440 && ch.r==0 && ch.g==0 && ch.b==0;
}
// Seleccion segura de un snapshot validado tras arranque o escritura cortada.
inline const ProfileImage* newestValid(const ProfileImage* a,const ProfileImage* b){
 const bool va=a&&curveValid(*a),vb=b&&curveValid(*b);
 if(va&&vb)return int32_t(a->generation-b->generation)>0?a:b;
 if(va)return a;
 if(vb)return b;
 return nullptr;
}
inline uint32_t fnvUpdate(uint32_t hash,const char* data,size_t len){
 for(size_t i=0;i<len;i++){hash^=(uint8_t)data[i];hash*=16777619UL;}
 return hash;
}
inline uint32_t wireHash(const char* pv,size_t pvLen,const char* ch,size_t chLen){
 uint32_t h=fnvUpdate(2166136261UL,pv,pvLen);
 h=fnvUpdate(h,"|",1);
 return fnvUpdate(h,ch,chLen);
}
inline bool expectedGeneration(uint32_t current,uint32_t expected){return current==expected;}
inline bool secondsModeSafe(bool wifiReady,bool bleWorkerReady,bool otaIdle,bool congested,
                             uint32_t internalHeap,uint32_t transportFailuresSinceStart){
 return wifiReady && bleWorkerReady && otaIdle && !congested &&
        internalHeap>=80000UL && transportFailuresSinceStart<3;
}
}}
