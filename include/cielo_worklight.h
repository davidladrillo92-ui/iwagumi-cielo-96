#pragma once
#include "cielo_engine.h"

// Temporary lighting only: no profile mutation, NVS, BLE or blocking waits.
namespace cielo96 { namespace worklight {
constexpr uint32_t RiseMs=600000UL,ReturnMs=120000UL,TickMs=10000UL;
enum class Phase:uint8_t {Idle,Rising,Holding,Returning};
struct Levels {cielo::PvValue pv; cielo::ChiValue ch;};
inline Levels target(){return {{96,150,210,15},{50,50,50}};}
inline uint32_t fraction(uint32_t elapsed,uint32_t duration){
 if(elapsed>=duration)return 1000;
 const uint64_t x=uint64_t(elapsed)*1000/duration;
 return uint32_t(x*x*(3000-2*x)/1000000); // smoothstep, bounded 0..1000
}
inline Levels blend(const Levels& a,const Levels& b,uint32_t f){
 return {{cielo::mix(a.pv.r,b.pv.r,f,1000),cielo::mix(a.pv.g,b.pv.g,f,1000),
          cielo::mix(a.pv.b,b.pv.b,f,1000),cielo::mix(a.pv.brightness,b.pv.brightness,f,1000)},
         {cielo::mix(a.ch.r,b.ch.r,f,1000),cielo::mix(a.ch.g,b.ch.g,f,1000),cielo::mix(a.ch.b,b.ch.b,f,1000)}};
}
class Session {
 Phase phase_=Phase::Idle;
 uint32_t begun_=0,transition_=0;
 Levels from_{};
public:
 bool active()const{return phase_!=Phase::Idle;}
 Phase phase()const{return phase_;}
 const char* phaseName()const{
  switch(phase_){case Phase::Rising:return "rising";case Phase::Holding:return "holding";
   case Phase::Returning:return "returning";default:return "idle";}
 }
 uint32_t elapsed(uint32_t now)const{return active()?uint32_t(now-begun_):0;}
 uint32_t remaining(uint32_t now)const{
  const uint32_t duration=phase_==Phase::Rising?RiseMs:phase_==Phase::Returning?ReturnMs:0;
  const uint32_t age=uint32_t(now-transition_);return age<duration?duration-age:0;
 }
 Levels output(uint32_t now,const Levels& program)const{
  if(phase_==Phase::Rising)return blend(from_,target(),fraction(now-transition_,RiseMs));
  if(phase_==Phase::Holding)return target();
  // Follow the CURRENT program during return, not the curve at session start.
  if(phase_==Phase::Returning)return blend(from_,program,fraction(now-transition_,ReturnMs));
  return program;
 }
 bool start(uint32_t now,const Levels& current){
  if(active())return false;
  from_=current;begun_=transition_=now;phase_=Phase::Rising;return true;
 }
 bool stop(uint32_t now,const Levels& program){
  if(!active()||phase_==Phase::Returning)return false; // repeated stop cannot extend return
  from_=output(now,program);transition_=now;phase_=Phase::Returning;return true;
 }
 bool advance(uint32_t now){
  if(phase_==Phase::Rising && uint32_t(now-transition_)>=RiseMs){phase_=Phase::Holding;return true;}
  if(phase_==Phase::Returning && uint32_t(now-transition_)>=ReturnMs){phase_=Phase::Idle;return true;}
  return false;
 }
 void cancel(){phase_=Phase::Idle;}
};
}} // cielo96::worklight
