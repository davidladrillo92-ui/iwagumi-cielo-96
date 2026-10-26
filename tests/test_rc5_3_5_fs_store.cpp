#include <cassert>
#include <cstring>
#include <iostream>
#include "cielo_profile_fs.h"
using namespace cielo96;

static ProfileImage sample(uint32_t gen){
 ProfileImage p{};
 p.generation=gen;p.pvCount=2;p.chCount=2;
 p.pv[0]={840,10,22,62,0};
 p.pv[1]={1440,58,78,132,0};
 p.ch[0]={870,1,1,1};
 p.ch[1]={1440,0,0,0};
 profileSeal(p);
 assert(curves::curveValid(p));return p;
}
int main(){
 profilefs::Store store;
 assert(store.begin());
 assert(store.ready() && store.initializedFromBlank());
 assert(SPIFFS.format_count==1);
 ProfileImage latest{};
 assert(!store.newest(latest));
 for(uint32_t g=9;g<=19;g++){
  auto p=sample(g);
  assert(store.save(p));
  assert(store.newest(latest));
  assert(latest.generation==g && memcmp(&latest,&p,sizeof(p))==0);
 }
 // Power interruption just before rename; currently active snapshot survives.
 SPIFFS.fail_rename=true;
 assert(!store.save(sample(20)));
 SPIFFS.fail_rename=false;
 assert(store.newest(latest) && latest.generation==19);
 // Interrupted/short temp write does not replace active curves.
 SPIFFS.fail_write=true;
 assert(!store.save(sample(20)));
 SPIFFS.fail_write=false;
 assert(store.newest(latest) && latest.generation==19);
 // Simulate reboot without erasing flash; latest saved version survives.
 profilefs::Store reboot;
 assert(reboot.begin() && reboot.ready());
 assert(reboot.newest(latest) && latest.generation==19);
 // A corrupt newest slot must allow recovery of the other one.
 auto newestPath=std::string(store.slot());
 assert(SPIFFS.exists(profilefs::kA) && SPIFFS.exists(profilefs::kB));
 // Both slots contain valid profiles; invalidate exactly the newest one.
 ProfileImage a{},b{};
 assert(store.read(profilefs::kA,a) && store.read(profilefs::kB,b));
 const char* newestFile=profilefs::newerGeneration(a.generation,b.generation)?profilefs::kA:profilefs::kB;
 SPIFFS.files[newestFile][0]^=0x01;
 assert(reboot.newest(latest) && latest.generation==18);
 // Under an UNMOUNTABLE, NONBLANK partition, never silently format data.
 SPIFFS=MockSPIFFS{};
 mock_flash[0]=0x42;
 profilefs::Store unsafe;
 assert(!unsafe.begin());
 assert(!unsafe.ready());
 assert(SPIFFS.format_count==0);
 assert(std::strcmp(unsafe.status(),"UNMOUNTABLE_NONEMPTY_PRESERVED")==0);
 std::cout<<"PASS RECOVERY3.5 dual-slot save/reboot/corruption/rename/short-write and safe mount\n";
}
