#include <cassert>
#include <cstring>
#include <iostream>
#include "cielo_profile_fs.h"
using namespace cielo96;
ProfileImage sample(unsigned gen){ProfileImage p{};p.generation=gen;p.pvCount=2;p.chCount=2;p.pv[0]={840,10,22,62,0};p.pv[1]={1440,58,78,132,0};p.ch[0]={870,1,1,1};p.ch[1]={1440,0,0,0};profileSeal(p);return p;}
void prepare(profilefs::Store&s){SPIFFS=MockSPIFFS{};std::fill(mock_flash.begin(),mock_flash.end(),0xff);assert(s.begin());assert(s.save(sample(9)));assert(s.save(sample(10)));assert(s.save(sample(11)));}
int main(){
 profilefs::Store store;prepare(store);ProfileImage a{},b{},latest{};
 assert(store.read(profilefs::kA,a)&&store.read(profilefs::kB,b));
 const char* active=a.generation>b.generation?profilefs::kA:profilefs::kB;
 const auto oldA=SPIFFS.files[profilefs::kA],oldB=SPIFFS.files[profilefs::kB];
 // Exact regression found in PR52: transient failure on the active read,
 // followed by rename failure used to delete accepted generation 11.
 SPIFFS.fail_read_path=active;SPIFFS.fail_read_once=true;SPIFFS.fail_rename=true;
 assert(!store.save(sample(12)));assert(strcmp(store.error(),"EXISTING_SLOT_UNREADABLE")==0);
 SPIFFS.fail_rename=false;
 assert(SPIFFS.files[profilefs::kA]==oldA&&SPIFFS.files[profilefs::kB]==oldB);
 assert(store.newest(latest)&&latest.generation==11&&!store.published());
 // A later read error during active recheck must also stop before removal.
 SPIFFS.read_calls=0;SPIFFS.fail_read_at=4;
 assert(!store.save(sample(12)));assert(strcmp(store.error(),"ACTIVE_RECHECK_FAILED")==0);
 assert(SPIFFS.files[profilefs::kA]==oldA&&SPIFFS.files[profilefs::kB]==oldB);
 SPIFFS.fail_read_at=0;
 // Reject duplicated/old generations, including an acknowledged-write retry.
 assert(!store.save(sample(11)));assert(strcmp(store.error(),"CANDIDATE_NOT_NEWER")==0);
 assert(!store.save(sample(10)));
 // A final read error may follow a successful publish: do not claim that
 // no new data persisted. Caller must report uncertainty and inspect/reboot.
 SPIFFS.read_calls=0;SPIFFS.fail_read_at=5;
 assert(!store.save(sample(12)));assert(strcmp(store.error(),"FINAL_VERIFY_FAILED")==0);
 assert(store.published());SPIFFS.fail_read_at=0;
 assert(store.newest(latest)&&latest.generation==12);
 // Future candidates still work; both snapshots stay independently valid.
 assert(store.save(sample(13))&&store.published());
 // A file present but CRC-corrupt cannot be treated as an empty slot.
 SPIFFS.files[profilefs::kA][0]^=1;
 const auto corruptA=SPIFFS.files[profilefs::kA],goodB=SPIFFS.files[profilefs::kB];
 assert(!store.save(sample(14)));assert(strcmp(store.error(),"EXISTING_SLOT_UNREADABLE")==0);
 assert(SPIFFS.files[profilefs::kA]==corruptA&&SPIFFS.files[profilefs::kB]==goodB);
 // Refuse unknown partitions and a nonblank byte even at the final offset.
 SPIFFS=MockSPIFFS{};mock_partition.address=0;profilefs::Store mismatch;
 assert(!mismatch.begin()&&SPIFFS.format_count==0);mock_partition.address=0x670000;
 mock_flash.back()=0x12;profilefs::Store nonblank;
 assert(!nonblank.begin()&&SPIFFS.format_count==0);
 std::cout<<"PASS FS audit regression: unreadable active + rename failure, active recheck, stale generation, uncertain publish, CRC and last-byte mount protection\n";
}
