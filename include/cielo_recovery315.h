#pragma once
#include <stdint.h>
namespace cielo96 { namespace r315 {
// Restart evidence remains dirty in RAM; failed flash attempts are bounded.
inline uint32_t restartRetryMs(bool failed){return failed?1800000UL:300000UL;}
// Calibration excludes cold discovery, failed operations and reconnections.
// A full window is required before freezing a reference.
struct StableLatency {
 uint32_t values[32]{}; uint8_t count=0,head=0;
 void observe(bool success,uint32_t ms,uint32_t discovery,uint32_t connect){
  if(!success||discovery||connect){count=0;head=0;return;}
  values[head]=ms;head=(head+1)%32;if(count<32)++count;
 }
 uint32_t p95()const{if(count!=32)return 0;uint32_t a[32];for(uint8_t i=0;i<32;++i)a[i]=values[i];
  for(uint8_t i=1;i<32;++i){uint32_t x=a[i];int j=i-1;while(j>=0&&a[j]>x){a[j+1]=a[j];--j;}a[j+1]=x;}return a[30];}
};
inline bool calibrationReady(uint32_t pv,uint32_t ch){return pv>0&&pv<=250&&ch>0&&ch<=1000;}
// Separate warm operation classes; no cold or failed result can seed a baseline.
struct LatencyClass {
 StableLatency window; uint32_t baseline=0; uint8_t bad=0,good=0; bool warning=false; bool fresh=false;
 void observe(bool eligible,uint32_t ms,uint32_t discovery,uint32_t connect){
  window.observe(eligible,ms,discovery,connect);fresh=true;
 }
 void sample(uint32_t ceiling){
  if(!fresh){bad=good=0;return;}
  fresh=false;
  const uint32_t p=window.p95();
  if(!p){bad=good=0;return;}
  if(!baseline){if(p<=ceiling)baseline=p;return;}
  if(p>ceiling&&uint64_t(p)>uint64_t(baseline)*3){good=0;if(bad<3)++bad;if(bad==3)warning=true;}
  else {bad=0;if(good<3)++good;if(good==3)warning=false;}
 }
};
enum class NightPlan {NotApplicable, WaitClock, DayTarget, NightOffCandidate};
inline NightPlan assessNight(bool powerOn,bool autoControl,bool takeover,bool trusted,bool profileValid,bool off){
 if(!powerOn||!autoControl||!takeover)return NightPlan::NotApplicable;
 if(!trusted||!profileValid)return NightPlan::WaitClock;
 return off?NightPlan::NightOffCandidate:NightPlan::DayTarget;
}
inline const char* nightPlanName(NightPlan p){
 switch(p){case NightPlan::WaitClock:return "WAIT_CLOCK";case NightPlan::DayTarget:return "DAY_TARGET";
 case NightPlan::NightOffCandidate:return "NIGHT_OFF_CANDIDATE";default:return "NOT_APPLICABLE";}
}
// Experimental policy ONLY. Not wired to BLE until optical bench validation.
enum class NightAction {None, ProbeMinimum, Zero};
struct NightProbe {
 enum class State {Idle,AwaitMinimum,NeedZero,AwaitZero,Done,Cancelled}; State state=State::Idle;
 NightAction start(bool powerOn,bool trusted,bool off,bool permitted){
  if(state!=State::Idle||!powerOn||!trusted||!off||!permitted)return NightAction::None;
  state=State::AwaitMinimum;return NightAction::ProbeMinimum;
 }
 void minimumResult(){if(state==State::AwaitMinimum)state=State::NeedZero;}
 NightAction next(bool trusted,bool off,bool permitted){
  if(state!=State::NeedZero||!trusted||!off||!permitted)return NightAction::None;
  state=State::AwaitZero;return NightAction::Zero;
 }
 void cancel(){if(state!=State::Done)state=State::Cancelled;}
 void zeroResult(){if(state==State::AwaitZero)state=State::Done;}
};
}}
