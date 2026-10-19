#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "cielo_engine.h"
namespace cielo96 {
// Versioned, validated, double-slot snapshot. Never overwrite the only valid copy.
struct ProfileImage {
 uint32_t magic=0xC1E10960UL;
 uint16_t format=1;
 uint16_t pvCount=0;
 uint16_t chCount=0;
 uint16_t reserved=0;
 uint32_t generation=0;
 cielo::PvPoint pv[64]{};
 cielo::ChiPoint ch[64]{};
 uint32_t checksum=0;
};
inline uint32_t profileHash(const ProfileImage& p){
 const uint8_t* b=reinterpret_cast<const uint8_t*>(&p);
 uint32_t h=2166136261UL;
 for(size_t i=0;i<offsetof(ProfileImage,checksum);++i){h^=b[i];h*=16777619UL;}
 return h;
}
inline bool profileValid(const ProfileImage&p){return p.magic==0xC1E10960UL&&p.format==1&&p.pvCount<=64&&p.chCount<=64&&
 cielo::valid(p.pv,p.pvCount)&&cielo::valid(p.ch,p.chCount)&&p.checksum==profileHash(p);}
inline void profileSeal(ProfileImage& p){p.checksum=profileHash(p);}
}
