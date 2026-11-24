#pragma once
// Recovery 3.14 V2: bounded read-only observations, no radio or storage access.
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace cielo96 { namespace health314 {
inline int offEvent(const char* s){
 if(!strcmp(s,"PV_POWER_OFF_ENCOLADO"))return 0;
 if(!strcmp(s,"PV_POWER_OFF_INTENTO"))return 1;
 if(!strcmp(s,"PV_POWER_OFF_GATT_OK"))return 2;
 return -1;
}
struct OffSummary {
 uint32_t count[3]{};uint64_t first[3]{},last[3]{};
 bool capture(const char* s,uint64_t at){
  int i=offEvent(s);if(i<0)return false;
  if(!count[i])first[i]=at;
  ++count[i];last[i]=at;return true;
 }
};
// No arbitrary event details; important events have independent retention.
struct Incident {uint64_t first=0,last=0;uint32_t repeats=0;char code[48]{};};
struct Incidents {
 Incident ring[16]{};uint8_t head=0,count=0;
 void capture(const char* code,uint64_t at){
  if(count){auto& e=ring[(head+15)%16];
   if(!strcmp(e.code,code)){++e.repeats;e.last=at;return;}
  }
  auto& e=ring[head];e=Incident{};e.first=e.last=at;e.repeats=1;
  strncpy(e.code,code,sizeof(e.code)-1);head=(head+1)%16;if(count<16)++count;
 }
 const Incident& at(uint8_t i)const{return ring[(head+16-count+i)%16];}
};
inline bool optionalWriteAllowed(bool statsValid,size_t free,size_t reserve,size_t margin){
 return statsValid && free>=reserve && free-reserve>=margin;
}
inline uint32_t diagnosticRetryMs(bool pressure){return pressure?1800000UL:300000UL;}
inline const char* pressureName(bool valid,size_t free,size_t reserve){
 return !valid?"UNKNOWN":free<reserve?"BELOW_RESERVE":free-reserve<16?"LOW_MARGIN":"NORMAL";
}
enum Flag : uint32_t { Heap=1, Fragmentation=2, PvLatency=4, ChLatency=8, Queue=16, Stack=32, Nvs=64 };
struct Sample {
 uint32_t heap=0,fragmentation=0,pv95=0,ch95=0,busyBp=0,stack=0,nvsFree=0,nvsReserve=0;
 bool nvsValid=false,latencyValid=false,queueValid=false;
};
struct Trend {
 Sample baseline{};uint32_t samples=0,flags=0;uint8_t streak[7]{},clear[7]{};
 void sample(const Sample& s){
  if(!s.heap)return;
  if(!samples){baseline=s;}
  // A cold or night-only first sample may not have enough GATT data.
  if(s.latencyValid&&!baseline.latencyValid){baseline.pv95=s.pv95;baseline.ch95=s.ch95;baseline.latencyValid=true;}
  if(s.nvsValid&&!baseline.nvsValid){baseline.nvsFree=s.nvsFree;baseline.nvsReserve=s.nvsReserve;baseline.nvsValid=true;}
  // Baseline frozen after first eligible warm sample. NVS margin tracked separately.
  ++samples;
  bool known[7]={true,true,s.latencyValid&&baseline.latencyValid,s.latencyValid&&baseline.latencyValid,s.queueValid,true,s.nvsValid};
  bool bad[7]={s.heap<60000 || uint64_t(s.heap)*100< uint64_t(baseline.heap)*80,
   s.fragmentation>=40 && s.fragmentation>=baseline.fragmentation+15,
   s.pv95>250 && uint64_t(s.pv95)>uint64_t(baseline.pv95)*3,
   s.ch95>1000 && uint64_t(s.ch95)>uint64_t(baseline.ch95)*3,
   s.busyBp>=2500,s.stack<3000,
   s.nvsFree<s.nvsReserve || (baseline.nvsValid&&baseline.nvsFree>=s.nvsFree&&baseline.nvsFree-s.nvsFree>=16)};
  for(uint8_t i=0;i<7;i++){
   if(!known[i]){streak[i]=0;clear[i]=0;continue;}
   if(bad[i]){clear[i]=0;if(streak[i]<3)++streak[i];if(streak[i]>=3)flags|=1UL<<i;}
   else{streak[i]=0;if(clear[i]<3)++clear[i];if(clear[i]>=3)flags&=~(1UL<<i);}
  }
 }
};
struct BleClass {
 uint32_t completed=0,startupSlow=0,reconnectSlow=0,stableSlow=0,recoveredSlow=0,failedSlow=0;
 uint32_t matchedSoftTimeoutOk=0,matchedSoftTimeoutFailed=0;
 void softResult(bool timedOut,bool success){if(timedOut){if(success)++matchedSoftTimeoutOk;else ++matchedSoftTimeoutFailed;}}
 uint64_t lastStableSlow=0;uint8_t consecutiveStableSlow=0;
 void observe(uint64_t at,bool success,uint32_t ms,uint32_t discovery,uint32_t connect){
  bool slow=ms>3500;bool first=completed==0;++completed;
  if(slow){
   if(success)++recoveredSlow;else ++failedSlow;
   if(first)++startupSlow;
   else if(discovery||connect)++reconnectSlow;
   else {++stableSlow;lastStableSlow=at;if(consecutiveStableSlow<255)++consecutiveStableSlow;return;}
  }
  consecutiveStableSlow=0;
 }
 bool recentWarning(uint64_t now)const{return consecutiveStableSlow>=2 && now-lastStableSlow<=300000ULL;}
};
struct Levels {uint8_t r=0,g=0,b=0,brightness=0;};
inline bool equal(Levels a,Levels b){return a.r==b.r&&a.g==b.g&&a.b==b.b&&a.brightness==b.brightness;}
inline bool equivalent(Levels a,Levels b,bool pv){return (pv&&!a.brightness&&!b.brightness)||equal(a,b);}
inline bool lit(Levels a,bool pv){return pv?a.brightness!=0:(a.r||a.g||a.b);}
inline uint16_t intensity(Levels a,bool pv){return pv?a.brightness:uint16_t(a.r)+a.g+a.b;}
inline const char* phase(Levels before,Levels now,Levels after,bool pv){
 if(!lit(now,pv))return "OFF";
 const auto n=intensity(now,pv),b=intensity(before,pv),a=intensity(after,pv);
 if(a>=n&&n>=b&&(a>n||n>b))return "RISING";
 if(a<=n&&n<=b&&(a<n||n<b))return "FALLING";
 if(!equal(before,now)||!equal(now,after))return "TRANSITION";
 return "STEADY";
}
struct Delivery {
 Levels sent{},ack{};uint32_t sequence=0,ackSeq=0;
 uint64_t sentAt=0,ackAt=0;uint32_t lastMs=0,maxMs=0,count=0,queueAge=0,lastTotalMs=0,maxTotalMs=0;
 bool pending=false,hasAck=false,hasSent=false,ackApplicable=false,fromProfile=false,queueAgeKnown=false,lastTotalKnown=false;
 void dispatch(uint32_t seq,Levels v,uint64_t at,bool profileMode=true,bool queueKnown=false,uint32_t queued=0){sequence=seq;sent=v;sentAt=at;pending=true;hasSent=true;fromProfile=profileMode;queueAgeKnown=queueKnown;queueAge=queued;}
 bool finish(uint32_t seq,bool matched,bool success, uint64_t at,bool forcedOff){
  if(!pending||seq!=sequence||!matched)return false;
  pending=false;if(!success){ackApplicable=false;return false;}
  ack=forcedOff?Levels{}:sent;ackSeq=seq;ackAt=at;hasAck=true;ackApplicable=fromProfile;
  lastMs=uint32_t(at-sentAt);if(lastMs>maxMs)maxMs=lastMs;
  lastTotalKnown=queueAgeKnown;if(lastTotalKnown){lastTotalMs=queueAge+lastMs;if(lastTotalMs>maxTotalMs)maxTotalMs=lastTotalMs;}++count;return true;
 }
 void invalidate(){ackApplicable=false;fromProfile=false;}
};
inline const char* summary(bool critical,bool observing,bool warning){
 return critical?"INCIDENTE_ACTUAL":observing?"OBSERVACION":warning?"OPERATIVO_CON_AVISOS":"OPERATIVO";
}
}} // namespace
