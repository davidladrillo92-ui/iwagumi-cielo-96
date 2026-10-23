#pragma once
#include <stdint.h>
namespace cielo96 { namespace recovery {
constexpr uint32_t STALL_MS=120000UL, REARM_MS=1800000UL;
constexpr uint32_t MAGIC=0xC1E1B143UL;
constexpr uint8_t MAX_RESTARTS=2;
struct Retained {
 uint32_t magic, attempts, pending, phase, device, age_ms, epoch, originBoot, check;
};
inline uint32_t checksum(const Retained&r){
 return MAGIC^r.attempts^r.pending^r.phase^r.device^r.age_ms^r.epoch^r.originBoot^0x947B5261UL;
}
inline void seal(Retained&r){r.magic=MAGIC;r.check=checksum(r);}
inline bool valid(const Retained&r){
 return r.magic==MAGIC && r.check==checksum(r) && r.attempts<=MAX_RESTARTS && r.pending<=1;
}
enum class Decision { None, Restart, Exhausted };
class Supervisor {
 bool tracking=false;uint32_t healthySince=0;
public:
 Decision tick(uint32_t now,bool ready,uint32_t heartbeatAge,uint32_t phaseAge,
               bool inhibited,Retained&r){
  const bool healthy=ready && heartbeatAge<15000UL && phaseAge<15000UL && !inhibited;
  if(!healthy)tracking=false;
  else if(!tracking){tracking=true;healthySince=now;}
  else if(r.attempts && uint32_t(now-healthySince)>=REARM_MS){r.attempts=0;seal(r);}
  if(!ready||inhibited||heartbeatAge<STALL_MS||phaseAge<STALL_MS)return Decision::None;
  return r.attempts<MAX_RESTARTS?Decision::Restart:Decision::Exhausted;
 }
};
inline void armRestart(Retained&r,uint8_t phase,uint8_t device,uint32_t age){
 ++r.attempts;r.pending=1;r.phase=phase;r.device=device;r.age_ms=age;seal(r);
}
} }
