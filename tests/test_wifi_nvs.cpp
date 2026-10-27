#include "cielo_wifi_nvs.h"
#include <iostream>
using namespace cielo96::wificfg;
Image network(const char*s,const char*p){Image v;assert(make(v,s,strlen(s),p,strlen(p),true));return v;}
void put(const char*k,const Image&i){const auto*p=reinterpret_cast<const uint8_t*>(&i);mocknvs::data[k]={p,p+sizeof(i)};}
int main(){
 using namespace mocknvs;
 const Image old=network("old-network","old-password"),fresh=network("new-network","new-password");
 reset();data["ssid"]={1,2,3};data["wifi_pw"]={4,5};data["curveA"]={6,7};data["restart_v2"]={8,9};const auto foreign=data;
 {NvsStore s;Image got;auto r=save(s,old,got);assert(r.verified&&r.degraded&&r.generation==1&&r.slot==0);s.close();assert(handles.empty());}
 for(const auto&kv:foreign)assert(data.at(kv.first)==kv.second);
 {NvsStore s;Image got;const int before=writes,cb=commits;
  for(int i=0;i<1000;i++){auto r=save(s,old,got);assert(r.verified&&r.unchanged);}
  assert(writes==before&&commits==cb);assert(s.write("ssid",old)==NvsStore::Corrupt);assert(s.read("wifi_pw",got)==NvsStore::Corrupt);
  auto r=save(s,fresh,got);assert(r.verified&&!r.degraded&&r.slot==1&&r.generation==2);}
 assert(handles.empty()&&opens==closes);
 const auto baseline=data;
 // Faults at each read phase (query+payload), write, commit and corrupt verification.
 for(int phase=0;phase<14;phase++){
  reset();data=baseline;Image ram=old,confirmed=old;NvsStore s;
  if(phase<10)readFailAt=phase+1;
  if(phase==10)writeError=ESP_ERR_NVS_NOT_ENOUGH_SPACE;
  if(phase==11)commitError=-104;
  if(phase==12){writeError=-105;failAfterWrite=true;}
  if(phase==13)mutateVerify=true;
  auto r=save(s,network("third-network","third-password"),confirmed);s.close();
  assert(!r.verified&&samePayload(confirmed,ram));
  assert(data.at("wifiB")==baseline.at("wifiB")); // Current survivor gen2 unchanged.
  for(const auto&kv:foreign)assert(data.at(kv.first)==kv.second);
  if(phase>=11)assert(r.mayHavePersisted);
  readFailAt=0;mutateVerify=false;auto reboot=inspect(s);
  assert(reboot.origin==Origin::Atomic);
  assert(samePayload(reboot.image,fresh)||samePayload(reboot.image,network("third-network","third-password")));
 }
 // Synthetic torn visible blobs (stronger than NVS API): complete survivor or AP-only on unknown schema.
 for(int cut=0;cut<112;cut++){
  reset();data=baseline;tornBytes=cut;NvsStore s;Image confirmed=old;
  auto r=save(s,network("third-network","third-password"),confirmed);s.close();assert(!r.verified);
  auto reboot=inspect(s);assert(reboot.origin==Origin::Blocked||(reboot.origin==Origin::Atomic&&samePayload(reboot.image,fresh)));
 }
 reset();{NvsStore s;Image out;data["wifiA"]={1};assert(s.read("wifiA",out)==NvsStore::Corrupt);openError=-10;assert(s.read("wifiB",out)==-10);}
 assert(handles.empty());
 std::cout<<"PASS Wi-Fi real NVS adapter: isolated handles, no-op1000, all read phases, commit/NoSpace/uncertain writes,112 torn replacements, unrelated keys preserved\n";
}
