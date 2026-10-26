#pragma once
// Same publication protocol is exercised with real LittleFS in the host lab.
// Single owner: HTTP/loop only. No callbacks, NVS access, remove or format API.
#include "cielo_curve_policy.h"
#include <stdio.h>
#include <string.h>
namespace cielo96 { namespace littlecurve {
constexpr const char* kA="/cielo_lfs_a.bin";
constexpr const char* kB="/cielo_lfs_b.bin";
constexpr unsigned kMaxStages=64;
enum class Read {Missing,Valid,Unreadable};
enum class Present {Missing,Exists,Error};
template<class IO> class Publisher {
 IO& io_;
 ProfileImage a_{},b_{},check_{};
 const ProfileImage* active_=nullptr;
 const char* activePath_=nullptr;
 bool scan(){
  active_=nullptr;activePath_=nullptr;readIssue="NONE";
  const Read a=io_.read(kA,a_),b=io_.read(kB,b_);
  if(a==Read::Unreadable||b==Read::Unreadable)readIssue="EXISTING_SLOT_UNREADABLE";
  if(a==Read::Valid&&b==Read::Valid){
   const uint32_t delta=a_.generation-b_.generation;
   if((delta==0&&memcmp(&a_,&b_,sizeof(a_))!=0)||delta==0x80000000UL){
    readIssue="GENERATION_CONFLICT";return false;
   }
  }
  active_=curves::newestValid(a==Read::Valid?&a_:nullptr,b==Read::Valid?&b_:nullptr);
  if(active_)activePath_=active_==&a_?kA:kB;
  return active_!=nullptr;
 }
 bool fail(const char*reason){error=reason;return false;}
public:
 const char* error="NONE";
 const char* readIssue="NONE";
 const char* slot="NONE";
 char stage[40]{};
 bool published=false,uncertain=false,unchanged=false;
 unsigned preservedStages=0;
 uint32_t saveAttempts=0,verifiedSaves=0,noops=0,bytesRequested=0;
 explicit Publisher(IO& io):io_(io){}
 bool newest(ProfileImage& out){if(!scan())return false;out=*active_;return true;}
 bool save(const ProfileImage& candidate){
  ++saveAttempts;error="NONE";slot="NONE";stage[0]=0;
  published=false;uncertain=false;unchanged=false;preservedStages=0;
  if(!curves::curveValid(candidate))return fail("INVALID_PROFILE");
  scan();
  if(strcmp(readIssue,"NONE")!=0)return fail(readIssue);
  if(active_){
   const uint32_t delta=candidate.generation-active_->generation;
   if(!delta){
    if(memcmp(&candidate,active_,sizeof(candidate))!=0)return fail("GENERATION_CONFLICT");
    unchanged=true;++noops;return true;
   }
   if(delta>=0x80000000UL)return fail("CANDIDATE_NOT_NEWER");
  }
  if(!io_.headroom())return fail("SPACE_HEADROOM_LOW");
  slot=active_==&a_?kB:kA;
  bool found=false;
  for(unsigned i=0;i<kMaxStages;++i){
   snprintf(stage,sizeof(stage),"/cielo_lfs_stage_%02u.bin",i);
   const Present p=io_.present(stage);
   if(p==Present::Error)return fail("STAGE_STAT_FAILED");
   if(p==Present::Missing){found=true;break;}
   ++preservedStages; // never open/truncate/remove an existing stage
  }
  if(!found){stage[0]=0;return fail("STAGING_LIMIT_REACHED");}
  bytesRequested+=sizeof(candidate);
  // O_EXCL closes the gap between stat and creation. An error keeps the file.
  if(!io_.createExclusive(stage,candidate))return fail("TEMP_WRITE_OR_SYNC_FAILED");
  if(io_.read(stage,check_)!=Read::Valid||memcmp(&candidate,&check_,sizeof(candidate))!=0)
   return fail("TEMP_VERIFY_FAILED");
  if(active_&&(io_.read(activePath_,check_)!=Read::Valid||memcmp(active_,&check_,sizeof(check_))!=0))
   return fail("ACTIVE_RECHECK_FAILED");
  // Rename atomically replaces only the inactive destination. No unlink gap.
  uncertain=true;
  if(!io_.replace(stage,slot))return fail("RENAME_FAILED");
  published=true;
  if(io_.read(slot,check_)!=Read::Valid||memcmp(&candidate,&check_,sizeof(candidate))!=0)
   return fail("FINAL_VERIFY_FAILED");
  uncertain=false;++verifiedSaves;return true;
 }
};
}}
