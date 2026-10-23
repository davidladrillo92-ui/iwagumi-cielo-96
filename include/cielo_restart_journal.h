#pragma once
#include <stdint.h>
#include <stddef.h>
namespace cielo96 { namespace restarts {
constexpr uint32_t MAGIC=0xC1E10601UL;
constexpr size_t CAPACITY=8;
// Fixed-size closed records; no addresses, free text or credentials.
struct Entry {
 uint32_t boot=0,previous=0,epoch=0,stallEpoch=0,stallMs=0,recoveredEpoch=0;
 uint8_t reset=0,phase=0,device=255,outcome=0; // 0 pending, 1 communication recovered
 bool bleRecovery=false;
 uint8_t reserved[3]{};
};
struct Journal {
 uint32_t magic=MAGIC;
 uint8_t count=0,head=0;
 uint16_t reserved=0;
 Entry entries[CAPACITY]{};
 uint32_t check=0;
};
inline uint32_t checksum(const Journal&j){
 const uint8_t*p=reinterpret_cast<const uint8_t*>(&j);uint32_t hash=2166136261UL;
 for(size_t i=0;i<offsetof(Journal,check);i++)hash=(hash^p[i])*16777619UL;
 return hash;
}
inline void seal(Journal&j){j.check=checksum(j);}
inline bool valid(const Journal&j){return j.magic==MAGIC&&j.count<=CAPACITY&&j.head<CAPACITY&&j.check==checksum(j);}
inline void append(Journal&j,const Entry&e){
 j.entries[j.head]=e;j.head=(j.head+1)%CAPACITY;if(j.count<CAPACITY)j.count++;seal(j);
}
inline const Entry&at(const Journal&j,size_t i){return j.entries[(j.head+CAPACITY-j.count+i)%CAPACITY];}
inline bool recovered(Journal&j,uint32_t boot,uint32_t epoch){
 if(!j.count)return false;
 Entry&e=j.entries[(j.head+CAPACITY-1)%CAPACITY];
 if(e.boot!=boot||e.outcome)return false;
 e.outcome=1;e.recoveredEpoch=epoch;seal(j);return true;
}
} }
