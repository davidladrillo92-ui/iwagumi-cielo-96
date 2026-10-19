#include "cielo_ble_queue.h"
#include <assert.h>
#include <stdio.h>
int main(){
 using namespace cielo::ble;
 Scheduler q(100,20,1000);Command cmd{};
 assert(q.submit(Device::PezVerde,Action::Apply,200,0,0,30,0,1000));
 assert(q.submit(Device::PezVerde,Action::Apply,210,0,0,40,1,1000));
 assert(q.metrics().superseded==1);
 assert(q.start_next(2,cmd));assert(cmd.r==210);
 q.tick(110);assert(q.busy()&&q.stalled()); // soft timeout does not release radio
 assert(!q.start_next(111,cmd));
 assert(q.complete(cmd.sequence,true,112));assert(!q.busy());
 assert(q.submit(Device::Chihiros,Action::RestoreAuto,0,0,0,0,120));
 assert(!q.submit(Device::Chihiros,Action::Apply,10,10,10,0,121));
 assert(q.start_next(122,cmd));assert(cmd.action==Action::RestoreAuto);
 assert(q.complete(cmd.sequence,true,130));
 puts("OK: dedup, serialized BLE, stall safety, mandatory AUTO priority");
}
