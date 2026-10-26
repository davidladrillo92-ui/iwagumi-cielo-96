// Host-only experiment. No filenames/images/USB ports accepted from the user.
// Nothing in this directory is included by the ESP32 firmware.
#include "lfs.h"
#include "cielo_curve_policy.h"
#include "v5_fixture.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>

using cielo96::ProfileImage;
constexpr size_t BlockSize=4096, BlockCount=384;
constexpr const char *A="/cielo_profile_a.bin", *B="/cielo_profile_b.bin";
constexpr const char *Temp="/cielo_profile_tmp.bin", *Foreign="/fixture_foreign.dat";
struct PowerCut {};
enum class Cut {Before, After, Partial};
struct Disk {
 std::vector<uint8_t> flash=std::vector<uint8_t>(BlockSize*BlockCount,255);
 // A synthetic NVS partition, deliberately outside every FS callback.
 std::vector<uint8_t> nvs=std::vector<uint8_t>(0x5000,0x5a);
 size_t mutations=0, programs=0, erases=0, syncs=0;
 size_t cutAt=0; Cut cut=Cut::Before;
 bool readError=false;
 void arm(size_t at=0,Cut mode=Cut::Before){mutations=programs=erases=syncs=0;cutAt=at;cut=mode;}
 bool start(){++mutations;if(mutations==cutAt&&cut==Cut::Before)throw PowerCut{};return mutations==cutAt;}
 static Disk& get(const lfs_config*c){return *static_cast<Disk*>(c->context);}
 static int read(const lfs_config*c,lfs_block_t b,lfs_off_t o,void*p,lfs_size_t n){
  auto&d=get(c);if(d.readError||b>=BlockCount||o>BlockSize||n>BlockSize-o)return LFS_ERR_IO;
  std::memcpy(p,d.flash.data()+b*BlockSize+o,n);return 0;
 }
 static int program(const lfs_config*c,lfs_block_t b,lfs_off_t o,const void*p,lfs_size_t n){
  auto&d=get(c);assert(b<BlockCount&&o<=BlockSize&&n<=BlockSize-o);
  const bool hit=d.start();++d.programs;size_t count=hit&&d.cut==Cut::Partial?n/2:n;
  const auto*bytes=static_cast<const uint8_t*>(p);
  for(size_t i=0;i<count;i++){auto&dest=d.flash[b*BlockSize+o+i];assert((dest&bytes[i])==bytes[i]);dest&=bytes[i];}
  if(hit)throw PowerCut{};
  return 0;
 }
 static int erase(const lfs_config*c,lfs_block_t b){
  auto&d=get(c);assert(b<BlockCount);const bool hit=d.start();++d.erases;
  size_t count=hit&&d.cut==Cut::Partial?BlockSize/2:BlockSize;
  std::fill_n(d.flash.begin()+b*BlockSize,count,255);if(hit)throw PowerCut{};return 0;
 }
 static int sync(const lfs_config*c){auto&d=get(c);const bool hit=d.start();++d.syncs;if(hit)throw PowerCut{};return 0;}
};

struct Session {
 lfs_t fs{};lfs_config cfg{};
 std::array<uint8_t,256> readCache{},progCache{};
 std::array<uint8_t,48> lookahead{};
 explicit Session(Disk&d){cfg.context=&d;cfg.read=Disk::read;cfg.prog=Disk::program;cfg.erase=Disk::erase;cfg.sync=Disk::sync;
  cfg.read_size=16;cfg.prog_size=16;cfg.block_size=BlockSize;cfg.block_count=BlockCount;cfg.block_cycles=500;
  cfg.cache_size=readCache.size();cfg.lookahead_size=lookahead.size();cfg.read_buffer=readCache.data();
  cfg.prog_buffer=progCache.data();cfg.lookahead_buffer=lookahead.data();}
 int mount(){return lfs_mount(&fs,&cfg);}
 int close(){return lfs_unmount(&fs);}
};

bool writeFile(Session&s,const char*path,const void*bytes,size_t size){
 lfs_file_t f{};std::array<uint8_t,256> cache{};lfs_file_config cfg{};cfg.buffer=cache.data();
 int e=lfs_file_opencfg(&s.fs,&f,path,LFS_O_WRONLY|LFS_O_CREAT|LFS_O_TRUNC,&cfg);if(e)return false;
 auto n=lfs_file_write(&s.fs,&f,bytes,size);int synced=lfs_file_sync(&s.fs,&f);int closed=lfs_file_close(&s.fs,&f);
 return n==static_cast<lfs_ssize_t>(size)&&!synced&&!closed;
}
enum class Read {Absent,Valid,Unreadable};
Read readProfile(Session&s,const char*path,ProfileImage&out){
 lfs_info info{};int e=lfs_stat(&s.fs,path,&info);if(e==LFS_ERR_NOENT)return Read::Absent;
 if(e||info.type!=LFS_TYPE_REG||info.size!=sizeof(out))return Read::Unreadable;
 lfs_file_t f{};std::array<uint8_t,256> cache{};lfs_file_config cfg{};cfg.buffer=cache.data();
 if(lfs_file_opencfg(&s.fs,&f,path,LFS_O_RDONLY,&cfg))return Read::Unreadable;
 auto n=lfs_file_read(&s.fs,&f,&out,sizeof(out));int closed=lfs_file_close(&s.fs,&f);
 return n==sizeof(out)&&!closed&&cielo96::curves::curveValid(out)?Read::Valid:Read::Unreadable;
}
bool equal(const ProfileImage&a,const ProfileImage&b){return std::memcmp(&a,&b,sizeof(a))==0;}
enum class Result {Saved,Unchanged,Refused};

// Experimental publication sequence, NOT a firmware backend. NVS is an immutable
// input to this experiment; no marker, key removal or generation increment.
Result publish(Session&s,const ProfileImage&candidate){
 if(!cielo96::curves::curveValid(candidate))return Result::Refused;
 ProfileImage a{},b{},temp{},check{};
 Read ra=readProfile(s,A,a),rb=readProfile(s,B,b);
 if(ra==Read::Unreadable||rb==Read::Unreadable)return Result::Refused;
 if(ra==Read::Valid&&rb==Read::Valid){
  uint32_t delta=a.generation-b.generation;
  if((delta==0&&!equal(a,b))||delta==0x80000000UL)return Result::Refused;
 }
 const auto*active=cielo96::curves::newestValid(ra==Read::Valid?&a:nullptr,rb==Read::Valid?&b:nullptr);
 if(active){
  uint32_t delta=candidate.generation-active->generation;
  if(!delta)return equal(candidate,*active)?Result::Unchanged:Result::Refused;
  if(delta>=0x80000000UL)return Result::Refused;
 }
 // Conservative experimental headroom: metadata and COW need working blocks.
 // This is not a promise that a byte-count check makes a write succeed.
 lfs_ssize_t used=lfs_fs_size(&s.fs);
 if(used<0||used>static_cast<lfs_ssize_t>(BlockCount-8))return Result::Refused;
 Read rt=readProfile(s,Temp,temp);
 // An unknown/partial temporary is preserved for explicit recovery; it is
 // never silently truncated. This may block migration, not the NVS fallback.
 if(rt==Read::Unreadable||(rt==Read::Valid&&!equal(temp,candidate)))return Result::Refused;
 if(rt==Read::Absent&&!writeFile(s,Temp,&candidate,sizeof(candidate)))return Result::Refused;
 if(readProfile(s,Temp,check)!=Read::Valid||!equal(check,candidate))return Result::Refused;
 const char*activePath=active==&a?A:B;
 if(active&&(readProfile(s,activePath,check)!=Read::Valid||!equal(check,*active)))return Result::Refused;
 const char*destination=active==&a?B:A;
 // LittleFS rename replaces the inactive file directly. Never remove active.
 if(lfs_rename(&s.fs,Temp,destination))return Result::Refused;
 if(readProfile(s,destination,check)!=Read::Valid||!equal(check,candidate))return Result::Refused;
 return Result::Saved;
}

std::vector<uint8_t> foreignBytes(){std::vector<uint8_t> v(282079);for(size_t i=0;i<v.size();i++)v[i]=uint8_t((i*17+11)%251);return v;}
void checkForeign(Session&s){
 auto expected=foreignBytes();lfs_file_t f{};std::array<uint8_t,256> cache{};lfs_file_config cfg{};cfg.buffer=cache.data();
 assert(!lfs_file_opencfg(&s.fs,&f,Foreign,LFS_O_RDONLY,&cfg));std::vector<uint8_t> actual(expected.size());
 assert(lfs_file_read(&s.fs,&f,actual.data(),actual.size())==static_cast<lfs_ssize_t>(actual.size()));
 assert(!lfs_file_close(&s.fs,&f));assert(actual==expected);
}
Disk prepared(const ProfileImage* a=nullptr,const ProfileImage*b=nullptr){
 Disk d;Session s(d);
 // Formatting exists ONLY to manufacture an empty synthetic fixture in RAM.
 assert(!lfs_format(&s.fs,&s.cfg));assert(!s.mount());auto data=foreignBytes();
 assert(writeFile(s,Foreign,data.data(),data.size()));
 if(a)assert(writeFile(s,A,a,sizeof(*a)));
 if(b)assert(writeFile(s,B,b,sizeof(*b)));
 auto*source=cielo96::curves::newestValid(a,b);
 if(source)std::memcpy(d.nvs.data()+128,source,sizeof(*source));
 assert(!s.close());d.arm();return d;
}
ProfileImage recovered(Session&s,const ProfileImage&fallback){
 ProfileImage a{},b{};bool va=readProfile(s,A,a)==Read::Valid,vb=readProfile(s,B,b)==Read::Valid;
 auto*p=cielo96::curves::newestValid(va?&a:nullptr,vb?&b:nullptr);return p?*p:fallback;
}
size_t testedCuts=0,blockedRetries=0;
void campaign(const Disk&baseline,const ProfileImage&candidate,const ProfileImage&fallback){
 Disk successful=baseline;Session normal(successful);assert(!normal.mount());successful.arm();
 ProfileImage originalA{},originalB{};
 bool preserveA=readProfile(normal,A,originalA)==Read::Valid&&equal(originalA,fallback);
 bool preserveB=readProfile(normal,B,originalB)==Read::Valid&&equal(originalB,fallback);
 assert(publish(normal,candidate)==Result::Saved);size_t ops=successful.mutations;assert(ops>0);assert(!normal.close());
 for(Cut mode:{Cut::Before,Cut::After,Cut::Partial})for(size_t point=1;point<=ops;point++){
  Disk trial=baseline;Session interrupted(trial);assert(!interrupted.mount());trial.arm(point,mode);bool cut=false;
  try{(void)publish(interrupted,candidate);}catch(const PowerCut&){cut=true;}
  assert(cut);assert(trial.nvs==baseline.nvs);trial.arm();
  // Discard all old lfs_t/cache/file objects. Remount the persisted bytes only.
  Session reboot(trial);assert(!reboot.mount());auto found=recovered(reboot,fallback);
  assert(equal(found,fallback)||equal(found,candidate));checkForeign(reboot);
  ProfileImage retained{};
  if(preserveA)assert(readProfile(reboot,A,retained)==Read::Valid&&equal(retained,fallback));
  if(preserveB)assert(readProfile(reboot,B,retained)==Read::Valid&&equal(retained,fallback));
  auto retried=publish(reboot,candidate);
  if(retried==Result::Refused){++blockedRetries;assert(equal(recovered(reboot,fallback),found));}
  else assert(equal(recovered(reboot,fallback),candidate));
  assert(trial.nvs==baseline.nvs);assert(!reboot.close());++testedCuts;
 }
 std::cout<<"scenario_mutation_boundaries="<<ops<<" program_calls="<<successful.programs
          <<" erase_calls="<<successful.erases<<" sync_calls="<<successful.syncs<<"\n";
}
void refusal(Disk d,const ProfileImage&candidate){
 Session s(d);assert(!s.mount());auto before=d.flash;auto nvs=d.nvs;d.arm();
 assert(publish(s,candidate)==Result::Refused);assert(d.mutations==0&&d.flash==before&&d.nvs==nvs);assert(!s.close());
}
void fullDisk(const ProfileImage&p,const ProfileImage&next){
 Disk d=prepared(&p);auto nvs=d.nvs;Session s(d);assert(!s.mount());
 lfs_file_t f{};std::array<uint8_t,256> cache{};lfs_file_config cfg{};cfg.buffer=cache.data();
 assert(!lfs_file_opencfg(&s.fs,&f,"/synthetic_filler.dat",LFS_O_WRONLY|LFS_O_CREAT,&cfg));
 std::array<uint8_t,256> bytes{};int failure=0;
 for(size_t i=0;i<BlockCount*16+1;i++){
  int n=lfs_file_write(&s.fs,&f,bytes.data(),bytes.size());
  if(n<0){failure=n;break;}assert(n==static_cast<int>(bytes.size()));
  int e=lfs_file_sync(&s.fs,&f);if(e){failure=e;break;}
 }
 assert(failure==LFS_ERR_NOSPC);(void)lfs_file_close(&s.fs,&f);
 assert(publish(s,next)==Result::Refused);assert(d.nvs==nvs);assert(!s.close());
 Session reboot(d);assert(!reboot.mount());assert(equal(recovered(reboot,p),p));
 checkForeign(reboot);assert(!reboot.close());
}
int main(int argc,char**){
 assert(argc==1);static_assert(sizeof(ProfileImage)==788,"profile wire size changed");
 ProfileImage p=fixtureV5();assert(p.generation==9&&p.pvCount==38&&p.chCount==31);
 assert(cielo96::curves::curveValid(p));ProfileImage old=p;old.generation=8;cielo96::profileSeal(old);
 ProfileImage next=p;next.generation=10;next.pv[2].r--;cielo96::profileSeal(next);
 auto blank=prepared();std::memcpy(blank.nvs.data()+128,&p,sizeof(p));campaign(blank,p,p);
 campaign(prepared(&old,&p),next,p);campaign(prepared(&p),next,p);
 Disk repeat=prepared(&p);Session s(repeat);assert(!s.mount());auto bytes=repeat.flash;repeat.arm();
 for(int i=0;i<1000;i++)assert(publish(s,p)==Result::Unchanged);
 assert(repeat.mutations==0&&repeat.flash==bytes);assert(!s.close());
 refusal(prepared(&p),old);ProfileImage bad=p;bad.checksum^=1;refusal(prepared(),bad);
 ProfileImage collision=p;collision.pv[2].r--;cielo96::profileSeal(collision);
 refusal(prepared(&p),collision);refusal(prepared(&p,&collision),next);
 ProfileImage ambiguous=p;ambiguous.generation=p.generation+0x80000000UL;cielo96::profileSeal(ambiguous);
 refusal(prepared(&p,&ambiguous),next);refusal(prepared(&p),ambiguous);
 Disk damaged=prepared(&p);{Session q(damaged);assert(!q.mount());uint8_t unknown[11]={1};
  assert(writeFile(q,B,unknown,sizeof(unknown)));assert(!q.close());}refusal(damaged,next);
 Disk temp=prepared(&p);{Session q(temp);assert(!q.mount());uint8_t unknown[11]={2};
  assert(writeFile(q,Temp,unknown,sizeof(unknown)));assert(!q.close());}refusal(temp,next);
 Disk otherTemp=prepared(&p);{Session q(otherTemp);assert(!q.mount());
  assert(writeFile(q,Temp,&old,sizeof(old)));assert(!q.close());}refusal(otherTemp,next);
 Disk io=prepared(&p);{Session q(io);assert(!q.mount());io.arm();io.readError=true;
  assert(publish(q,next)==Result::Refused&&io.mutations==0);io.readError=false;assert(!q.close());}
 // Recover an already verified temporary; do not write it a second time.
 Disk ready=prepared(&p);{Session q(ready);assert(!q.mount());assert(writeFile(q,Temp,&next,sizeof(next)));assert(!q.close());}
 campaign(ready,next,p);
 fullDisk(p,next);
 Disk blankFlash;auto before=blankFlash.flash;Session unformatted(blankFlash);
 assert(unformatted.mount()<0&&blankFlash.mutations==0&&blankFlash.flash==before);
 Disk unreadable=prepared(&p);before=unreadable.flash;unreadable.readError=true;Session failedRead(unreadable);
 assert(failedRead.mount()<0&&unreadable.mutations==0&&unreadable.flash==before);
 // RFC1982-style wrap around is accepted only within the unambiguous half range.
 ProfileImage last=p;last.generation=std::numeric_limits<uint32_t>::max();cielo96::profileSeal(last);
 ProfileImage wrapped=p;wrapped.generation=0;cielo96::profileSeal(wrapped);
 Disk wrap=prepared(&last);Session w(wrap);assert(!w.mount());assert(publish(w,wrapped)==Result::Saved);
 assert(equal(recovered(w,last),wrapped));assert(!w.close());
 std::cout<<"{\"synthetic_only\":true,\"disk_version\":"<<LFS_DISK_VERSION
          <<",\"profile_bytes\":788,\"power_cut_cases\":"<<testedCuts
          <<",\"retries_preserved_for_review\":"<<blockedRetries
          <<",\"identical_saves\":1000,\"identical_save_mutations\":0,\"nvs_changed\":false,\"foreign_file_preserved\":true"
          <<",\"full_disk_preserved\":true,\"blank_and_read_error_not_formatted\":true}\n";
}
