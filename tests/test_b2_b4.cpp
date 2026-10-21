#include <initializer_list>
#include "cielo_engine.h"
#include "cielo_profile_store.h"
#include "cielo_ble_queue.h"
#include "profiles_generated.h"
#include <cassert>
#include <cstdio>
#include <cstring>

int main(){
 using namespace cielo;
 // At every whole minute (30s stable), subminute path must match the legacy curve.
 for(unsigned t=0;t<=1440;t++){
  auto pv=at(pezverde,pezverde_count,t);
  auto sec=atSecond(pezverde,pezverde_count,t*60UL);
  assert(pv.r==sec.r&&pv.g==sec.g&&pv.b==sec.b&&pv.brightness==sec.brightness);
  auto ch=at(chihiros,chihiros_count,t);
  auto cs=atSecond(chihiros,chihiros_count,t*60UL);
  assert(ch.r==cs.r&&ch.g==cs.g&&ch.b==cs.b);
 }
 // 15s gives genuinely finer visible values; no extra control points or memory writes.
 PvPoint ramp[]={{870,0,0,0,0},{871,60,60,60,60},{1440,0,0,0,0}};
 assert(valid(ramp,3));
 assert(atSecond(ramp,3,870*60+15).r==15);
 assert(atSecond(ramp,3,870*60+30).r==30);
 assert(atSecond(ramp,3,870*60+45).r==45);
 assert(at(ramp,3,870).r==0);
 // Exact moon requested by user: 22:59 2/2/2 directly to 23:00 1/1/2.
 ChiPoint moon[]={{1379,2,2,2},{1380,1,1,2},{1440,0,0,0}};
 assert(valid(moon,3));
 for(unsigned delta:{0U,15U,30U,45U,59U}){
  auto v=atSecond(moon,3,1379*60+delta);
  assert(v.r==2&&v.g==2&&v.b==2);
 }
 auto after=atSecond(moon,3,1380*60);assert(after.r==1&&after.g==1&&after.b==2);
 // 64 points per lamp / profile checksum and memory bounded.
 cielo96::ProfileImage p{};p.pvCount=64;p.chCount=64;
 for(unsigned i=0;i<64;i++){
  auto m=static_cast<uint16_t>(i==63?1440:870+i);
  p.pv[i]={m,static_cast<uint8_t>(i),30,40,static_cast<uint8_t>(i==63?0:30)};
  p.ch[i]={m,static_cast<uint8_t>(i),30,40};
 }
 p.ch[63].r=p.ch[63].g=p.ch[63].b=0;
 assert(valid(p.pv,64)&&valid(p.ch,64));
 cielo96::profileSeal(p);assert(cielo96::profileValid(p));
 static_assert(sizeof(cielo96::ProfileImage)<2048,"Profile snapshot exceeds safe NVS size");
 p.pv[1].r++;assert(!cielo96::profileValid(p));
 // Model NVS double-slot recovery: a partial/bad write does not erase last valid.
 cielo96::ProfileImage a{};a.pvCount=pezverde_count;a.chCount=chihiros_count;
 std::memcpy(a.pv,pezverde,sizeof(pezverde));std::memcpy(a.ch,chihiros,sizeof(chihiros));
 cielo96::profileSeal(a);assert(cielo96::profileValid(a));
 cielo96::ProfileImage b=a;b.generation=1;b.pv[1].r++;
 cielo96::profileSeal(b);assert(cielo96::profileValid(b));
 b.checksum^=0x100;assert(!cielo96::profileValid(b));
 assert(cielo96::profileValid(a)); // slot A survived when slot B corrupt
 // No concurrent BLE operations: queued PV and CH serialize; stale PV is cancelled.
 cielo::ble::Scheduler q(3500,150,2000);
 assert(q.submit(cielo::ble::Device::PezVerde,cielo::ble::Action::Apply,70,40,20,30,100));
 assert(q.submit(cielo::ble::Device::Chihiros,cielo::ble::Action::Apply,20,20,20,0,100));
 cielo::ble::Command cmd{};
 assert(q.start_next(100,cmd));assert(!q.start_next(100,cmd));
 assert(q.complete(cmd.sequence,true,200));
 assert(q.start_next(300,cmd));assert(q.complete(cmd.sequence,true,400));
 // If the worker is temporarily occupied, latest interpolation supersedes stale colour.
 cielo::ble::Scheduler other(3500,150,2000);
 assert(other.submit(cielo::ble::Device::PezVerde,cielo::ble::Action::Apply,10,20,30,40,100));
 assert(other.submit(cielo::ble::Device::PezVerde,cielo::ble::Action::Apply,15,25,35,40,115));
 assert(other.metrics().superseded==1);
 assert(other.start_next(115,cmd));assert(cmd.r==15&&cmd.g==25&&cmd.b==35);
 assert(other.complete(cmd.sequence,true,120));
 // Cancel pending old colour as night obligation takes precedence.
 assert(q.submit(cielo::ble::Device::PezVerde,cielo::ble::Action::Apply,80,40,20,40,500));
 assert(q.cancel_pending(cielo::ble::Device::PezVerde));
 assert(q.submit(cielo::ble::Device::PezVerde,cielo::ble::Action::Apply,0,0,0,0,501));
 assert(q.start_next(501,cmd));assert(cmd.brightness==0);
 std::puts("B2+B4: 15s interpolation, 30s parity, 64 points, direct moon, atomic recovery model, BLE queue PASS");
}
