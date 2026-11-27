#pragma once
#include <stdint.h>
namespace cielo96 { namespace dev2 {
// Main-loop only. No radio ownership, allocations, flash writes or global resets.
inline uint32_t retryDelay(uint8_t n){
 const uint32_t delays[]={5000,15000,30000,60000,120000,300000};
 return delays[n>6?5:n?n-1:0];
}
enum class Result { Untracked, Stale, Accepted, Failed };
struct Recovery {
 bool waiting=false, valid=false; uint8_t streak=0;
 uint32_t due=0, sequence=0, episodes=0, attempts=0, recovered=0, failures=0, stale=0, losses=0, rejects=0;
 bool ready(uint32_t now)const{return waiting&&!sequence&&int32_t(now-due)>=0;}
 bool permits(uint32_t now)const{return !waiting||ready(now);}
 uint32_t remaining(uint32_t now)const{return waiting&&int32_t(now-due)<0?due-now:0;}
 void backoff(uint32_t now){if(!waiting)++episodes;waiting=true;if(streak<6)++streak;due=now+retryDelay(streak);}
 void loss(uint32_t now){++losses;valid=false;if(!waiting)backoff(now);}
 void dispatch(uint32_t seq){sequence=seq;valid=true;if(waiting)++attempts;}
 Result complete(uint32_t seq,bool matched,bool ok,uint32_t now,bool urgentOff=false){
  if(!sequence||seq!=sequence)return Result::Untracked;
  if(!matched)return Result::Stale; // do not invalidate the live sequence on an unrelated response
  sequence=0;
  if(!valid){++stale;return Result::Stale;}
  valid=false;
  if(ok){if(waiting)++recovered;waiting=false;streak=0;return Result::Accepted;}
  ++failures;
  if(!urgentOff)backoff(now); // OFF uses the existing independent safety retry policy
  return Result::Failed;
 }
 void invalidate(){valid=false;}
 void rejected(uint32_t now,bool urgentOff){++rejects;if(!urgentOff)backoff(now);}
 void cancel(){waiting=false;valid=false;streak=0;} // keep last seq to reject late completion
 const char* state()const{return waiting?(valid?"PROBING":sequence?"DRAINING":"BACKOFF"):(valid?"IN_FLIGHT":"NORMAL");}
};
}}
