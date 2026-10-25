#pragma once
#include <stdint.h>
namespace cielo96 { namespace cadence {
struct Diagnostics {
 uint32_t due=0,busy=0,pendingOnly=0,longPhase=0,evaluated=0,offPriority=0,invalidClock=0,inhibited=0;
 void sample(bool inflight,bool pending,uint32_t phaseAge){
  due++;if(inflight)busy++;else if(pending)pendingOnly++;
  if(inflight&&phaseAge>=15000UL)longPhase++;
 }
};
} }
