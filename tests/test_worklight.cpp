#include "cielo_worklight.h"
#include "cielo_night_safety.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace cielo96::worklight;
int main(){
 const Levels dark{{0,0,0,0},{0,0,0}},day{{240,238,230,25},{70,65,60}};
 Session s;assert(!s.active());assert(s.output(100,dark).ch.r==0);
 assert(s.start(100,dark));assert(!s.start(200,day));
 auto first=s.output(100,dark);assert(first.pv.brightness==0&&first.ch.r==0);
 uint8_t last=0;
 for(uint32_t t=0;t<=RiseMs;t+=1000){
  const auto v=s.output(100+t,dark);assert(v.ch.r>=last&&v.ch.r<=50);
  assert(v.ch.r==v.ch.g&&v.ch.g==v.ch.b&&v.pv.brightness<=15);last=v.ch.r;
 }
 auto mid=s.output(100+RiseMs/2,dark);assert(mid.ch.r==25&&mid.pv.brightness==8);
 assert(s.advance(100+RiseMs));assert(s.phase()==Phase::Holding);
 assert(s.output(900000,day).ch.r==50); // saved curve does not interrupt maintenance
 assert(s.stop(900000,dark));assert(!s.stop(950000,dark));assert(s.remaining(950000)==70000);
 assert(s.output(900000,dark).ch.r==50);
 assert(s.output(960000,dark).ch.r==25);
 assert(s.output(1020000,dark).ch.r==0);
 assert(s.advance(1020000)&&!s.active());
 // Stop during ascent is continuous. Daytime stop returns to actual current curve.
 assert(s.start(0,dark));auto before=s.output(300000,dark);
 assert(s.stop(300000,dark));auto after=s.output(300000,day);
 assert(memcmp(&before,&after,sizeof(before))==0);
 auto final=s.output(420000,day);assert(final.ch.r==70&&final.pv.brightness==25);
 assert(s.advance(420000)&&!s.active());
 assert(s.start(0xfffffff0u,dark));auto wrap=s.output(uint32_t(0xfffffff0u+RiseMs),dark);
 assert(wrap.ch.r==50);assert(s.advance(uint32_t(0xfffffff0u+RiseMs)));
 s.cancel();assert(!s.active());assert(s.output(12,dark).ch.r==0);
 Session reboot;assert(!reboot.active()); // RAM-only: cannot restore forgotten session after reboot
 assert(cielo96::safety::offRequired(true,true,true,s.output(12,dark).pv.brightness));
 assert(!s.stop(12,dark));assert(fraction(0,RiseMs)==0&&fraction(RiseMs,RiseMs)==1000);
 std::cout<<"PASS worklight: full ascent, hold, early/repeated stop, moving return curve, dark OFF, cancel, reboot default and millis rollover\n";
}
