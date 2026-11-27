#pragma once
#include "cielo_history314.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
namespace cielo96 { namespace history315 {
using history314::Day; using history314::Ring;
static constexpr uint32_t periodMs=21600000UL, retryMs=1800000UL, bootQuietMs=900000UL;
struct Image { uint32_t magic=0,version=0,generation=0,epoch=0,count=0; Day days[30]{}; uint32_t crc=0; };
static_assert(sizeof(Day)==28 && sizeof(Image)==864,"stable history format");
inline uint32_t checksum(const Image& x){
 const uint8_t* p=reinterpret_cast<const uint8_t*>(&x);uint32_t c=~0U;
 for(size_t i=0;i<offsetof(Image,crc);++i){c^=p[i];for(unsigned b=0;b<8;++b)c=(c>>1)^(0xedb88320U & (0U-(c&1U)));}return ~c;
}
inline bool validDay(uint32_t k){const uint32_t y=k/1000,d=k%1000;return y>=2026&&y<=2100&&d<365U+uint32_t(y%4==0&&(y%100!=0||y%400==0));}
inline bool valid(const Image& x){
 if(x.magic!=0x43483531U||x.version!=1||!x.generation||x.epoch<1700000000UL||x.epoch>4102444800UL||!x.count||x.count>30||x.crc!=checksum(x))return false;
 for(uint32_t i=0;i<x.count;++i){const Day& d=x.days[i];if(!validDay(d.day_key)||!d.samples||!d.heap_min||(i&&d.day_key<=x.days[i-1].day_key))return false;}
 return true;
}
inline void snapshot(Image& x,const Ring& r,uint32_t gen,uint32_t epoch){x=Image{};x.magic=0x43483531U;x.version=1;x.generation=gen;x.epoch=epoch;x.count=r.count();for(uint32_t i=0;i<x.count;++i)x.days[i]=r.oldest(i);x.crc=checksum(x);}
enum class Read { Missing, Valid, Invalid, Error, Foreign };
class Posix {
 const char* root_;
 bool path(unsigned slot,char* p,size_t n)const{const int k=snprintf(p,n,"%s/cielo_hist315_%c.bin",root_,slot?'b':'a');return k>0&&size_t(k)<n;}
public:
 explicit Posix(const char* root="/cielo-lfs"):root_(root){}
 Read read(unsigned slot,Image& out){
  char p[128];if(!path(slot,p,sizeof(p)))return Read::Error;
  struct stat st{};if(::stat(p,&st))return errno==ENOENT?Read::Missing:Read::Error;
  if(!S_ISREG(st.st_mode))return Read::Foreign;
  if(st.st_size>static_cast<off_t>(sizeof(out)))return Read::Foreign;
  if(st.st_size<8)return Read::Invalid;
  int fd=::open(p,O_RDONLY);if(fd<0)return Read::Error;size_t done=0;bool ok=true;
  out=Image{};const size_t expected=size_t(st.st_size);
  while(done<expected){ssize_t n=::read(fd,reinterpret_cast<uint8_t*>(&out)+done,expected-done);if(n<0&&errno==EINTR)continue;if(n<=0){ok=false;break;}done+=size_t(n);}
  if(::close(fd))ok=false;
  if(!ok)return Read::Error;
  if(out.magic!=0x43483531U||out.version!=1)return Read::Foreign;
  return expected==sizeof(out)&&valid(out)?Read::Valid:Read::Invalid;
 }
 bool write(unsigned slot,const Image& x){
  char p[128];if(!path(slot,p,sizeof(p)))return false;
  int fd=::open(p,O_WRONLY|O_CREAT|O_TRUNC,0600);if(fd<0)return false;size_t done=0;bool ok=true;
  while(done<sizeof(x)){ssize_t n=::write(fd,reinterpret_cast<const uint8_t*>(&x)+done,sizeof(x)-done);if(n<0&&errno==EINTR)continue;if(n<=0){ok=false;break;}done+=size_t(n);}
  if(::fsync(fd))ok=false;
  if(::close(fd))ok=false;
  return ok;
 }
};
template<class IO> class Store {
 IO& io_; Image scratch_{}; // single reusable workspace; no allocation on heap
 uint64_t lastAttempt_=0,lastSaved_=0; bool attempted_=false,saved_=false;
public:
 int active=-1; uint32_t generation=0,epoch=0,attempts=0,saves=0,failures=0,deferrals=0;
 bool loaded=false,blocked=false,dirty=false; const char* status="NOT_LOADED";
 explicit Store(IO& io):io_(io){}
 bool load(Ring& r){
  if(loaded)return !blocked;
  loaded=true;const Read a=io_.read(0,scratch_);uint32_t ga=0,ca=0;
  if(a==Read::Valid){ga=scratch_.generation;ca=scratch_.crc;}
  const Read b=io_.read(1,scratch_);uint32_t gb=b==Read::Valid?scratch_.generation:0;
  const bool uncertain=a==Read::Error||b==Read::Error||a==Read::Foreign||b==Read::Foreign;
  if(ga&&ga==gb&&ca!=scratch_.crc){blocked=true;status="READ_CONFLICT_PRESERVED";return false;}
  if(uncertain&&!ga&&!gb){blocked=true;status="READ_CONFLICT_PRESERVED";return false;}
  if(!ga&&!gb){blocked=a!=Read::Missing||b!=Read::Missing;status=blocked?"NO_VALID_COPY_PRESERVED":"EMPTY";return !blocked;}
  active=ga>=gb?0:1;
  if(active==0&&(io_.read(0,scratch_)!=Read::Valid||scratch_.generation!=ga||scratch_.crc!=ca)){blocked=true;status="REREAD_FAILED";return false;}
  if(!r.restore(scratch_.days,scratch_.count)){blocked=true;status="RESTORE_FAILED";return false;}
  generation=scratch_.generation;epoch=scratch_.epoch;blocked=uncertain;status=uncertain?"RESTORED_READ_ONLY_CONFLICT":(a==Read::Invalid||b==Read::Invalid)?"RECOVERED_OTHER_SLOT":"RESTORED";return true;
 }
 bool due(uint64_t now,uint32_t trustedEpoch)const{
  return loaded&&!blocked&&dirty&&now>=bootQuietMs&&trustedEpoch>=1700000000UL&&trustedEpoch<=4102444800UL&&
   (!epoch||(trustedEpoch>=epoch&&trustedEpoch-epoch>=periodMs/1000))&&
   (!attempted_||now-lastAttempt_>=retryMs)&&(!saved_||now-lastSaved_>=periodMs);
 }
 bool checkpoint(const Ring& r,uint64_t now,uint32_t trustedEpoch,bool headroom){
  if(!due(now,trustedEpoch)||!r.count())return false;
  attempted_=true;lastAttempt_=now;
  if(!headroom){++deferrals;status="LOW_SPACE_OR_INFO_ERROR";return false;}
  if(generation==UINT32_MAX){blocked=true;status="GENERATION_EXHAUSTED";return false;}
  ++attempts;const unsigned target=active==0?1:0;
  snapshot(scratch_,r,generation+1,trustedEpoch);const uint32_t expected=scratch_.crc;
  if(!valid(scratch_)||!io_.write(target,scratch_)||io_.read(target,scratch_)!=Read::Valid||scratch_.crc!=expected){++failures;status="CHECKPOINT_FAILED_PREVIOUS_PRESERVED";return false;}
  active=int(target);generation=scratch_.generation;epoch=scratch_.epoch;dirty=false;saved_=true;lastSaved_=now;++saves;status="CHECKPOINT_VERIFIED";return true;
 }
};
}}
