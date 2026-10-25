#include "cielo_ble_connection.h"
#include "cielo_restart_journal.h"
#include "cielo_ble_queue.h"
#include "cielo_cadence_diagnostics.h"
#include "cielo_security_policy.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
int main(){
 // Missing callback must cancel by 8 seconds on either target, including rollover.
 for(int device=0;device<2;device++)for(uint32_t start:{0U,0xfffffff0U}){
  uint32_t now=start;unsigned cancelled=0;
  assert(!cielo96::connection::await(start,[&]{return now;},[]{return 0;},[&]{cancelled++;},[&]{now+=20;}));
  assert(uint32_t(now-start)==8000&&cancelled==1);
  now=start;cancelled=0;
  assert(cielo96::connection::await(start,[&]{return now;},[&]{return uint32_t(now-start)>=300?1:0;},[&]{cancelled++;},[&]{now+=20;}));
  assert(cancelled==0&&uint32_t(now-start)==300);
  assert(!cielo96::connection::await(now,[&]{return now;},[]{return -1;},[&]{cancelled++;},[]{assert(false);}));
  assert(cancelled==1);
 }
 using namespace cielo96::restarts;
 Journal j{};seal(j);assert(valid(j));
 for(uint32_t id=109;id<=120;id++){
  Entry e{};e.boot=id;e.previous=id-1;e.reset=3;e.phase=4;e.device=id%2;e.bleRecovery=true;e.stallMs=120001;
  append(j,e);assert(valid(j));
 }
 assert(j.count==8&&at(j,0).boot==113&&at(j,7).boot==120);
 assert(recovered(j,120,1792750000)&&!recovered(j,120,1792750001));
 assert(at(j,7).outcome==1&&valid(j));
 j.entries[0].stallMs^=1;assert(!valid(j));
 using namespace cielo::ble;
 Scheduler q(3500,0,2000);Command c;
 q.submit(Device::Chihiros,Action::Apply,1,1,1,0,0);assert(q.start_next(0,c));q.complete(c.sequence,true,1);
 // Give normal round robin CH priority, then force safety OFF to go first.
 q.submit(Device::PezVerde,Action::Apply,1,1,1,25,2);assert(q.start_next(2,c));q.complete(c.sequence,true,3);
 q.submit(Device::Chihiros,Action::Apply,2,2,2,0,4);
 q.submit(Device::PezVerde,Action::Apply,0,0,0,0,4);
 assert(q.start_next(4,c,true)&&c.device==Device::PezVerde&&c.brightness==0);
 q.complete(c.sequence,false,5);
 // Failure cooldown must still allow the other screen to run.
 assert(q.start_next(6,c,true)&&c.device==Device::Chihiros);
 cielo96::cadence::Diagnostics d;
 d.sample(true,true,120001);d.sample(false,true,0);d.sample(false,false,0);
 assert(d.due==3&&d.busy==1&&d.pendingOnly==1&&d.longPhase==1);
 using cielo96::security::localHost;
 assert(localHost("192.168.0.25","192.168.0.25","192.168.4.1"));
 assert(localHost("IWAGUMI-CIELO96.LOCAL:80","192.168.0.25","192.168.4.1"));
 assert(localHost("192.168.4.1:80","0.0.0.0","192.168.4.1"));
 for(const char*h:{"attacker.test","192.168.0.25.attacker.test","192.168.0.25:8080","0.0.0.0","iwagumi-cielo96.local:80.evil"})
  assert(!localHost(h,"192.168.0.25","192.168.4.1"));
 puts("PASS RC6: bounded connection, journal retention/corruption, OFF priority/fairness, cadence and LAN Host");
}
