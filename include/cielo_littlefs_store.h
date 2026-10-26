#pragma once
#include "cielo_littlefs_readonly.h"
#include "cielo_littlefs_transaction.h"
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
namespace cielo96 { namespace littlecurve {
class PosixIO {
 bool mounted_=false;
 const char* root_;
 bool path(const char* name,char* out,size_t size){
  return name&&snprintf(out,size,"%s%s",root_,name)>0&&strlen(out)<size-1;
 }
public:
 int lastErrno=0;
 size_t total=0,used=0;
 uint32_t createCalls=0,writeBytes=0,syncCalls=0,renameCalls=0;
 explicit PosixIO(const char*root="/cielo-lfs"):root_(root){}
 void mounted(bool value){mounted_=value;}
 bool info(){
  if(!mounted_)return false;
  const esp_err_t e=esp_littlefs_info("spiffs",&total,&used);
  if(e!=ESP_OK||!total||used>total){lastErrno=EIO;return false;}return true;
 }
 bool headroom(){if(!info())return false;if(total-used<8*4096){lastErrno=ENOSPC;return false;}return true;}
 Present present(const char*name){
  char p[96];struct stat st{};
  if(!mounted_||!path(name,p,sizeof(p))){lastErrno=EINVAL;return Present::Error;}
  if(::stat(p,&st)==0)return Present::Exists;
  if(errno==ENOENT)return Present::Missing;
  lastErrno=errno;return Present::Error;
 }
 Read read(const char*name,ProfileImage&out){
  char p[96];struct stat st{};
  if(!mounted_||!path(name,p,sizeof(p))){lastErrno=EINVAL;return Read::Unreadable;}
  if(::stat(p,&st)){if(errno==ENOENT)return Read::Missing;lastErrno=errno;return Read::Unreadable;}
  if(!S_ISREG(st.st_mode)||st.st_size!=sizeof(out)){lastErrno=EINVAL;return Read::Unreadable;}
  const int fd=::open(p,O_RDONLY);if(fd<0){lastErrno=errno;return Read::Unreadable;}
  size_t done=0;bool ok=true;
  while(done<sizeof(out)){
   const ssize_t n=::read(fd,reinterpret_cast<uint8_t*>(&out)+done,sizeof(out)-done);
   if(n<0&&errno==EINTR)continue;
   if(n<=0){lastErrno=n<0?errno:EIO;ok=false;break;}done+=size_t(n);
  }
  if(::close(fd)){lastErrno=errno;ok=false;}
  if(ok&&curves::curveValid(out))return Read::Valid;
  if(ok)lastErrno=EILSEQ;
  return Read::Unreadable;
 }
 bool createExclusive(const char*name,const ProfileImage&candidate){
  char p[96];if(!mounted_||!path(name,p,sizeof(p)))return false;
  ++createCalls;const int fd=::open(p,O_WRONLY|O_CREAT|O_EXCL,0600);
  if(fd<0){lastErrno=errno;return false;}
  size_t done=0;bool ok=true;
  while(done<sizeof(candidate)){
   const ssize_t n=::write(fd,reinterpret_cast<const uint8_t*>(&candidate)+done,sizeof(candidate)-done);
   if(n<0&&errno==EINTR)continue;
   if(n<=0){lastErrno=n<0?errno:EIO;ok=false;break;}done+=size_t(n);writeBytes+=size_t(n);
  }
  ++syncCalls;if(::fsync(fd)){lastErrno=errno;ok=false;}
  if(::close(fd)){lastErrno=errno;ok=false;}return ok;
 }
 bool replace(const char*from,const char*to){
  char a[96],b[96];if(!mounted_||!path(from,a,sizeof(a))||!path(to,b,sizeof(b)))return false;
  ++renameCalls;const int e=::rename(a,b);if(e)lastErrno=errno;return !e;
 }
};
class Store {
 PosixIO io_;
 Publisher<PosixIO> txn_;
 bool ready_=false,blocked_=false;
 const char* status_="NOT_INITIALIZED";
 const char* blockedReason_="NONE";
public:
 esp_err_t mountError=ESP_OK;
 bool migrationAttempted=false,migrationVerified=false;
 explicit Store():txn_(io_){}
 bool begin(const storage::LittlefsReadonlyProbe&probe){
  if(!probe.verified||probe.retainedReadonlyMount){status_="READ_ONLY_PROBE_REQUIRED";return false;}
  if(esp_littlefs_mounted("spiffs")){status_="MOUNT_OWNERSHIP_CONFLICT";return false;}
  esp_vfs_littlefs_conf_t c{};c.base_path="/cielo-lfs";c.partition_label="spiffs";
  c.read_only=false;c.format_if_mount_failed=false;c.grow_on_mount=false;c.dont_mount=false;
  mountError=esp_vfs_littlefs_register(&c);
  if(mountError!=ESP_OK){status_="WRITE_MOUNT_FAILED_PRESERVED";return false;}
  ready_=true;io_.mounted(true);status_="LITTLEFS_MOUNTED_EXISTING";
  if(!io_.info()){blocked_=true;blockedReason_="MOUNT_INFO_FAILED";}
  return ready_;
 }
 bool ready()const{return ready_;}
 void block(const char*reason){blocked_=true;blockedReason_=reason;}
 bool blocked()const{return blocked_;}
 const char* status()const{return blocked_?blockedReason_:status_;}
 const char* error()const{return blocked_?blockedReason_:txn_.error;}
 const char* readIssue()const{return txn_.readIssue;}
 const char* slot()const{return txn_.slot;}
 const char* stage()const{return txn_.stage;}
 unsigned preservedStages()const{return txn_.preservedStages;}
 bool published()const{return txn_.published;}
 bool uncertain()const{return txn_.uncertain;}
 bool newest(ProfileImage&out){
  if(!ready_)return false;
  const bool valid=txn_.newest(out);
  if(strcmp(txn_.readIssue,"NONE")!=0)block(txn_.readIssue);
  return valid;
 }
 bool save(const ProfileImage&candidate){io_.lastErrno=0;return ready_&&!blocked_&&txn_.save(candidate);}
 bool migrate(const ProfileImage&candidate){
  migrationAttempted=true;migrationVerified=save(candidate);return migrationVerified;
 }
 size_t total(){io_.info();return io_.total;}
 size_t used(){io_.info();return io_.used;}
 int lastErrno()const{return io_.lastErrno;}
 uint32_t saveAttempts()const{return txn_.saveAttempts;}
 uint32_t verifiedSaves()const{return txn_.verifiedSaves;}
 uint32_t createCalls()const{return io_.createCalls;}
 uint32_t bytesWritten()const{return io_.writeBytes;}
 uint32_t syncCalls()const{return io_.syncCalls;}
 uint32_t renameCalls()const{return io_.renameCalls;}
};
}}
