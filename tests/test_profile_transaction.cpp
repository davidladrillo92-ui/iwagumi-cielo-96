#include "cielo_profile_transaction.h"
#include <cassert>
#include <cstring>
#include <map>
#include <string>
#include <array>
#include <stdexcept>
#include <iostream>
using cielo96::ProfileImage;
using namespace cielo96;
struct Cut:std::exception{};
struct Store {
 static constexpr int Ok=0,NoSpace=0x1105,Unsafe=0x103,Corrupt=0x109,Missing=0x1102,Io=0x101;
 std::map<std::string,ProfileImage> records;
 // Five 126-entry pages, one reserved for GC. 474 live entries and 156
 // reported free exactly like report 77. Four used pages have 10/10/10/0
 // spare entries: no initial blob chunk reaches the SDK's ~400-byte threshold.
 std::array<int,4> used{{116,116,116,126}};
 std::map<std::string,int> page{{"curvesA",0},{"curvesB",1}};
 int calls=0,cutAt=0,reads=0,failRead=0,writes=0,eraseCalls=0,commits=0,failCommit=0;
 bool allWritesFull=false,ioWrite=false,badWrite=false,publishedThenError=false;
 std::string erased;
 void step(){if(++calls==cutAt)throw Cut();}
 int read(const char* key,ProfileImage& out){
  step();if(++reads==failRead)return Io;
  auto it=records.find(key);if(it==records.end())return Missing;
  out=it->second;return Ok;
 }
 int write(const char* key,const ProfileImage& value){
  step();++writes;if(ioWrite)return Io;if(allWritesFull)return NoSpace;
  // Separate allocator model, not the retry implementation under test.
  int best=0;for(int i=1;i<4;i++)if(used[i]<used[best])best=i;
  const size_t entries=nvssafe::minimumFreeEntries(sizeof(ProfileImage));
  const int tail=(126-used[best]-1)*32;
  if(tail<400 || 126-used[best]<int(entries))return NoSpace;
  // Copy-on-write consumes the new span before the previous span is retired.
  used[best]+=int(entries);
  if(records.count(key))used[page[key]]-=int(entries);
  page[key]=best;records[key]=value;
  if(badWrite)records[key].checksum^=1;
  step();return publishedThenError?Io:Ok;
 }
 int erase(const char* key){
  step();++eraseCalls;erased=key;
  auto it=records.find(key);if(it==records.end())return Missing;
  used[page[key]]-=int(nvssafe::minimumFreeEntries(sizeof(ProfileImage)));
  records.erase(it);step();return Ok;
 }
 int commit(){step();return ++commits==failCommit?Io:Ok;}
};
ProfileImage image(uint32_t generation,unsigned pvCount=34){
 ProfileImage p{};p.generation=generation;p.pvCount=pvCount;p.chCount=31;
 for(unsigned i=0;i<pvCount;i++){p.pv[i].minute=i*1440/(pvCount-1);p.pv[i].r=240;p.pv[i].g=238;p.pv[i].b=230;p.pv[i].brightness=i&&i+1<pvCount?25:0;}
 for(unsigned i=0;i<p.chCount;i++){p.ch[i].minute=i*1440/(p.chCount-1);p.ch[i].r=i+1<p.chCount?70:0;p.ch[i].g=i+1<p.chCount?65:0;p.ch[i].b=i+1<p.chCount?60:0;}
 profileSeal(p);assert(curves::curveValid(p));return p;
}
Store initial(){Store s;s.records["curvesA"]=image(7);s.records["curvesB"]=image(8);return s;}
bool valid(const Store&s,const char* key){auto it=s.records.find(key);return it!=s.records.end()&&curves::curveValid(it->second);}
int main(){
 auto active=image(8),candidate=image(9,38);assert(sizeof(ProfileImage)==788);
 {
  auto s=initial();assert(s.used[0]+s.used[1]+s.used[2]+s.used[3]==474);
  assert(s.write("curvesA",candidate)==Store::NoSpace); // reproduce pre-fix failure
  auto result=profiletxn::save(s,"curvesA","curvesB",active,candidate,true);
  assert(result.verified&&result.recoveryAttempted&&result.inactiveReclaimed);
  assert(result.initialWriteError==Store::NoSpace&&s.erased=="curvesA");
  assert(memcmp(&s.records["curvesB"],&active,sizeof(active))==0);
  assert(memcmp(&s.records["curvesA"],&candidate,sizeof(candidate))==0);
 }
 // Simulated API boundaries can lose power. At least the untouched active
 // record survives, and reboot can only select the complete old or new image.
 auto reference=initial();(void)profiletxn::save(reference,"curvesA","curvesB",active,candidate,true);
 for(int cut=1;cut<=reference.calls;cut++){
  auto s=initial();s.cutAt=cut;
  bool cutReached=false;try{(void)profiletxn::save(s,"curvesA","curvesB",active,candidate,true);}catch(const Cut&){cutReached=true;}assert(cutReached);
  assert(valid(s,"curvesB"));assert(memcmp(&s.records["curvesB"],&active,sizeof(active))==0);
  auto a=valid(s,"curvesA")?&s.records["curvesA"]:nullptr;
  auto latest=curves::newestValid(a,&s.records["curvesB"]);
  assert(latest&&(latest->generation==8||latest->generation==9));
 }
 for(int failure=1;failure<=3;failure++){
  auto s=initial();s.failRead=failure;
  auto r=profiletxn::save(s,"curvesA","curvesB",active,candidate,true);
  assert(!r.verified&&valid(s,"curvesB"));if(failure<3)assert(s.eraseCalls==0);
 }
 for(int failure=1;failure<=2;failure++){
  auto s=initial();s.failCommit=failure;
  auto r=profiletxn::save(s,"curvesA","curvesB",active,candidate,true);
  assert(!r.verified&&valid(s,"curvesB"));
 }
 {
  auto s=initial();s.badWrite=true;
  auto r=profiletxn::save(s,"curvesA","curvesB",active,candidate,true);
  assert(!r.verified&&r.failure==nvssafe::Failure::VerificationFailed&&valid(s,"curvesB"));
 }
 {
  auto s=initial();s.allWritesFull=true;
  auto r=profiletxn::save(s,"curvesA","curvesB",active,candidate,true);
  assert(!r.verified&&s.writes==2&&s.eraseCalls==1&&valid(s,"curvesB"));
 }
 {
  auto s=initial();s.ioWrite=true;
  auto r=profiletxn::save(s,"curvesA","curvesB",active,candidate,true);
  assert(!r.verified&&s.eraseCalls==0&&r.writeMayHavePersisted);
 }
 {
  auto s=initial();s.used={{50,50,50,50}};s.publishedThenError=true;
  auto r=profiletxn::save(s,"curvesA","curvesB",active,candidate,true);
  assert(!r.verified&&!r.writeAccepted&&r.writeMayHavePersisted);
  assert(s.eraseCalls==0&&s.records["curvesA"].generation==9&&valid(s,"curvesB"));
 }
 {
  auto s=initial();s.records.erase("curvesA");
  auto r=profiletxn::save(s,"curvesA","curvesB",active,candidate,false);
  assert(!r.verified&&s.eraseCalls==0&&valid(s,"curvesB")); // never reclaim the only good copy
 }
 for(const char* target:{"curvesB","ssid","wifi_pw","restart_v2"}){
  auto s=initial();auto r=profiletxn::save(s,target,"curvesB",active,candidate,true);
  assert(!r.verified&&s.writes==0&&s.eraseCalls==0);
 }
 {
  auto s=initial();auto stale=image(6);auto r=profiletxn::save(s,"curvesA","curvesB",stale,candidate,true);
  assert(!r.verified&&s.eraseCalls==0);
 }
 // Ten consecutive saves alternate snapshots without any unbounded NVS growth.
 {
  auto s=initial();auto current=active;
  for(unsigned i=0;i<10;i++){
   const char* target=i%2?"curvesB":"curvesA";const char* survivor=i%2?"curvesA":"curvesB";
   auto next=image(9+i,38);auto r=profiletxn::save(s,target,survivor,current,next,true);
   assert(r.verified&&valid(s,target)&&valid(s,survivor));current=next;
   assert(s.used[0]+s.used[1]+s.used[2]+s.used[3]==474);
  }
 }
 {
  auto s=initial();s.records["curvesA"]=image(0xfffffffeu);s.records["curvesB"]=image(0xffffffffu);
  auto r=profiletxn::save(s,"curvesA","curvesB",s.records["curvesB"],image(0),true);
  assert(r.verified);
 }
 assert(nvssafe::diagnosticReserve(788)==188);
 std::cout<<"PASS actual profile transaction: report-77 allocator model, all modeled API cut points, read/commit/CRC errors, single-copy preservation, key whitelist, rollover, 10 bounded saves\n";
}
