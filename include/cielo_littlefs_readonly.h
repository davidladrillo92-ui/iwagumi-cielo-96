#pragma once
#include <esp_partition.h>
#include <esp_littlefs.h>
#include <string.h>

namespace cielo96 { namespace storage {
// Boot-time inspection only. Never returns a writable filesystem to curves.
// The pinned SDK must expose all policy fields below; otherwise compilation
// fails, rather than silently falling back to a writable/growing mount.
class LittlefsReadonlyProbe {
 bool ran_=false;
public:
 const char* status="NOT_RUN";
 bool signature0=false,signature1=false,mountAttempted=false,verified=false;
 bool retainedReadonlyMount=false;
 esp_err_t readError=ESP_OK,mountError=ESP_OK,infoError=ESP_OK,unmountError=ESP_OK;
 size_t totalBytes=0,usedBytes=0;
 const char* driverVersion()const{
#ifdef ESP_LITTLEFS_VERSION_NUMBER
  return ESP_LITTLEFS_VERSION_NUMBER;
#else
  return "UNREPORTED";
#endif
 }
 void run(){
  if(ran_)return;
  ran_=true;
  const esp_partition_t* part=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,"spiffs");
  if(!part||part->address!=0x670000UL||part->size!=0x180000UL){status="PARTITION_MISMATCH_SKIPPED";return;}
  if(part->encrypted){status="ENCRYPTED_PARTITION_SKIPPED";return;}
  uint8_t prefix[16]{};
  readError=esp_partition_read(part,0,prefix,sizeof(prefix));
  if(readError!=ESP_OK){status="SIGNATURE_READ_FAILED";return;}
  signature0=memcmp(prefix+8,"littlefs",8)==0;
  readError=esp_partition_read(part,4096,prefix,sizeof(prefix));
  if(readError!=ESP_OK){status="SIGNATURE_READ_FAILED";return;}
  signature1=memcmp(prefix+8,"littlefs",8)==0;
  if(!signature0&&!signature1){status="SIGNATURE_NOT_DETECTED";return;}
  // Never unregister a filesystem mounted by another component.
  if(esp_littlefs_mounted("spiffs")){status="ALREADY_MOUNTED_SKIPPED";return;}
  esp_vfs_littlefs_conf_t conf{};
  conf.base_path="/cielo-lfs-ro";conf.partition_label="spiffs";conf.partition=nullptr;
  conf.format_if_mount_failed=false;conf.read_only=true;
  conf.dont_mount=false;conf.grow_on_mount=false;
  mountAttempted=true;mountError=esp_vfs_littlefs_register(&conf);
  if(mountError!=ESP_OK){status="READ_ONLY_MOUNT_FAILED";return;}
  retainedReadonlyMount=true;
  infoError=esp_littlefs_info("spiffs",&totalBytes,&usedBytes);
  unmountError=esp_vfs_littlefs_unregister("spiffs");
  if(unmountError==ESP_OK)retainedReadonlyMount=false;
  if(unmountError!=ESP_OK){status="READ_ONLY_UNMOUNT_FAILED";return;}
  if(infoError!=ESP_OK){status="READ_ONLY_INFO_FAILED";return;}
  if(!totalBytes||totalBytes>part->size||usedBytes>totalBytes){status="READ_ONLY_INFO_INVALID";return;}
  verified=true;status="READ_ONLY_VERIFIED_AND_CLOSED";
 }
};
}}
