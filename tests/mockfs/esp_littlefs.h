#pragma once
#include "esp_partition.h"
#include <cassert>
#include <cstring>
#define ESP_LITTLEFS_VERSION_NUMBER "MOCK_1.14.1"
struct esp_vfs_littlefs_conf_t {
 const char* base_path=nullptr;const char* partition_label=nullptr;const esp_partition_t* partition=nullptr;
 bool format_if_mount_failed=false,read_only=false,dont_mount=false,grow_on_mount=false;
};
inline unsigned lfs_register_calls=0,lfs_unregister_calls=0,lfs_info_calls=0;
inline bool lfs_mounted=false;
inline esp_err_t lfs_register_error=ESP_OK,lfs_info_error=ESP_OK,lfs_unregister_error=ESP_OK;
inline size_t lfs_total=0x180000,lfs_used=72*4096;
inline bool esp_littlefs_mounted(const char*label){assert(strcmp(label,"spiffs")==0);return lfs_mounted;}
inline esp_err_t esp_vfs_littlefs_register(const esp_vfs_littlefs_conf_t*c){
 ++lfs_register_calls;
 assert(strcmp(c->base_path,"/cielo-lfs-ro")==0&&strlen(c->base_path)<=15);
 assert(strcmp(c->partition_label,"spiffs")==0&&!c->partition);
 assert(c->read_only&&!c->format_if_mount_failed&&!c->grow_on_mount&&!c->dont_mount);
 if(lfs_register_error==ESP_OK)lfs_mounted=true;
 return lfs_register_error;
}
inline esp_err_t esp_littlefs_info(const char*label,size_t*t,size_t*u){
 assert(strcmp(label,"spiffs")==0&&lfs_mounted);++lfs_info_calls;*t=lfs_total;*u=lfs_used;return lfs_info_error;
}
inline esp_err_t esp_vfs_littlefs_unregister(const char*label){
 assert(strcmp(label,"spiffs")==0);++lfs_unregister_calls;
 if(lfs_unregister_error==ESP_OK)lfs_mounted=false;
 return lfs_unregister_error;
}
