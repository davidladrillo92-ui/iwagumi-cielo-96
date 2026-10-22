#pragma once
#include <stdint.h>
namespace cielo96 { namespace autonomy {
// A deliberate pause is stored as false; missing settings default to autonomous control.
inline bool enabled(bool stored,bool value){return !stored||value;}
struct Retry {
 bool pending=false,recovering=false;uint8_t episodes=0;uint32_t due=0,attempts=0;
 void arm(uint32_t now){
  recovering=true;if(pending)return;
  if(episodes<5)episodes++;
  uint32_t delay=30000UL<<(episodes?episodes-1:0);
  if(delay>300000UL)delay=300000UL;
  due=now+delay;pending=true;
 }
 bool ready(uint32_t now,bool permitted)const{
  return pending&&permitted&&static_cast<int32_t>(now-due)>=0;
 }
 void defer(uint32_t now){due=now+30000UL;}
 void begin(){pending=false;attempts++;}
 void success(){pending=false;recovering=false;episodes=0;}
 void cancel(){pending=false;recovering=false;episodes=0;}
};
} }
