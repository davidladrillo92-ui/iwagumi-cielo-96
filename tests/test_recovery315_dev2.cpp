#include "cielo_recovery315_dev2.h"
#include "cielo_ble_queue.h"
#include <cassert>
#include <cstdio>
using namespace cielo96::dev2;
using namespace cielo::ble;
int main(){
 Recovery pv,ch;
 // First ordinary failure is isolated; success on PV cannot reset CH.
 ch.dispatch(1);assert(ch.complete(1,true,false,100)==Result::Failed);
 assert(ch.waiting&&!ch.ready(5099)&&ch.ready(5100)&&pv.permits(100));
 pv.dispatch(2);assert(pv.complete(2,true,true,200)==Result::Accepted&&ch.waiting);
 const uint32_t delays[]={15000,30000,60000,120000,300000,300000};
 for(unsigned i=0;i<6;++i){auto now=ch.due;ch.dispatch(3+i);assert(!ch.permits(now));
 assert(ch.complete(3+i,true,false,now)==Result::Failed);assert(ch.due-now==delays[i]);}
 ch.dispatch(10);assert(ch.complete(9,false,true,ch.due)==Result::Untracked);
 assert(ch.complete(10,false,true,ch.due)==Result::Stale&&ch.valid);
 assert(ch.complete(10,true,true,ch.due)==Result::Accepted&&!ch.waiting&&ch.streak==0);
 assert(ch.attempts==7&&ch.recovered==1&&ch.episodes==1);
 // Link loss invalidates the result from the previous link generation.
 ch.dispatch(11);ch.loss(400);assert(!ch.ready(5400));assert(ch.complete(11,true,true,500)==Result::Stale&&ch.waiting);
 assert(ch.ready(5400));ch.dispatch(12);ch.cancel();assert(ch.complete(12,true,false,600)==Result::Stale&&!ch.waiting);
 // OFF remains urgent, failed OFF does not replace the safety backoff.
 pv.dispatch(13);assert(pv.complete(13,true,false,100,true)==Result::Failed&&!pv.waiting);
 pv.loss(0xfffffff0u);assert(!pv.ready(0xfffffff1u)&&pv.ready(4984));
 pv.dispatch(14);assert(pv.complete(14,true,true,5000,true)==Result::Accepted&&!pv.waiting);
 pv.rejected(6000,false);assert(pv.waiting&&pv.rejects==1&&pv.due==11000);pv.cancel();
 // Real scheduler: failed RGB is discarded, healthy peer can proceed; new target only.
 Scheduler q;Command cmd{};
 assert(q.submit(Device::Chihiros,Action::Apply,22,22,22,0,100));
 assert(q.start_next(100,cmd));ch.dispatch(cmd.sequence);
 assert(q.submit(Device::PezVerde,Action::Apply,70,65,60,20,101));
 assert(q.complete(cmd.sequence,false,102));assert(ch.complete(cmd.sequence,true,false,102)==Result::Failed);
 assert(q.cancel_pending(Device::Chihiros));assert(q.start_next(103,cmd)&&cmd.device==Device::PezVerde);
 assert(q.complete(cmd.sequence,true,104));assert(!q.start_next(200,cmd));
 assert(ch.ready(5102));assert(q.submit(Device::Chihiros,Action::Apply,0,0,0,0,5102));
 assert(q.start_next(5102,cmd)&&cmd.r==0);ch.dispatch(cmd.sequence);
 assert(q.complete(cmd.sequence,true,5103));assert(ch.complete(cmd.sequence,true,true,5103)==Result::Accepted);
 puts("PASS DEV2: isolated retry, capped backoff, stale results, OFF exception, real scheduler and fresh RGB");
}
