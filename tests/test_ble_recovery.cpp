#include "cielo_ble_recovery.h"
#include <cassert>
#include <type_traits>
#include <cstdio>
using namespace cielo96::recovery;
int main(){
 static_assert(std::is_trivial<Retained>::value,"RTC NOINIT must not run an initializer at boot");
 Retained r{};seal(r);assert(valid(r));Supervisor s;
 // Informe 42: normal idle or 6.716 s initial discovery is not a hard stall.
 assert(s.tick(7017000,true,113,113,false,r)==Decision::None);
 assert(s.tick(7018000,true,6716,6716,false,r)==Decision::None);
 assert(s.tick(7019000,true,120000,119999,false,r)==Decision::None);
 assert(s.tick(7020000,true,119999,120000,false,r)==Decision::None);
 // Informe 41: blocked in connect, not a radio-idle disconnect.
 assert(s.tick(25375000,true,1359609,1358846,false,r)==Decision::Restart);
 assert(s.tick(25375000,true,1359609,1358846,true,r)==Decision::None);
 assert(s.tick(25375000,false,1359609,1358846,false,r)==Decision::None);
 armRestart(r,4,0,1358846);assert(valid(r)&&r.pending==1&&r.attempts==1);
 r.pending=0;seal(r);Supervisor nextBoot;
 assert(nextBoot.tick(120000,true,120000,120000,false,r)==Decision::Restart);
 armRestart(r,4,0,120000);r.pending=0;seal(r);Supervisor thirdBoot;
 assert(thirdBoot.tick(120000,true,120000,120000,false,r)==Decision::Exhausted);
 // A quick good result must not replenish the restart budget.
 thirdBoot.tick(130000,true,100,100,false,r);
 thirdBoot.tick(130000+REARM_MS-1,true,100,100,false,r);assert(r.attempts==2);
 thirdBoot.tick(130000+REARM_MS,true,100,100,false,r);assert(r.attempts==0&&valid(r));
 armRestart(r,4,0,120000);r.pending=0;seal(r);Supervisor wrapped;
 wrapped.tick(0xfffffff0u,true,100,100,false,r);
 wrapped.tick(uint32_t(0xfffffff0u+REARM_MS),true,100,100,false,r);assert(r.attempts==0);
 armRestart(r,4,0,120000);r.pending=0;seal(r);Supervisor interrupted;
 interrupted.tick(1,true,0,0,false,r);
 interrupted.tick(REARM_MS-1,true,16000,16000,false,r);
 interrupted.tick(REARM_MS+1,true,0,0,false,r);assert(r.attempts==1);
 r.check^=1;assert(!valid(r));
 puts("PASS recovery: reports 41/42, timeout, inhibit, RTC, budget and millis rollover");
}
