#pragma once
#include "cielo_wifi_config.h"
#include <nvs.h>
namespace cielo96 { namespace wificfg {
class NvsStore {
 nvs_handle_t writing=0;bool open=false;
public:
 static constexpr int Ok=ESP_OK,Missing=ESP_ERR_NVS_NOT_FOUND,NoSpace=ESP_ERR_NVS_NOT_ENOUGH_SPACE,Corrupt=-31001;
 uint32_t writeAttempts=0,commits=0,bytesAccepted=0;
 ~NvsStore(){close();}
 void close(){if(open){nvs_close(writing);open=false;}}
 static bool allowed(const char*k){return k&&(strcmp(k,"wifiA")==0||strcmp(k,"wifiB")==0);}
 int read(const char*k,Image& out){
  if(!allowed(k))return Corrupt;
  nvs_handle_t h=0;int err=nvs_open("cielo96",NVS_READONLY,&h);if(err!=Ok)return err;
  size_t length=0;err=nvs_get_blob(h,k,nullptr,&length);
  if(err==Ok&&length!=sizeof(Image))err=Corrupt;
  if(err==Ok){out=Image{};err=nvs_get_blob(h,k,&out,&length);if(err==Ok&&length!=sizeof(Image))err=Corrupt;}
  nvs_close(h);return err;
 }
 int write(const char*k,const Image& value){
  if(!allowed(k)||!valid(value))return Corrupt;
  close();int err=nvs_open("cielo96",NVS_READWRITE,&writing);if(err!=Ok)return err;
  open=true;writeAttempts++;
  err=nvs_set_blob(writing,k,&value,sizeof(value));if(err==Ok)bytesAccepted+=sizeof(value);return err;
 }
 int commit(){if(!open)return ESP_ERR_NVS_INVALID_HANDLE;commits++;return nvs_commit(writing);}
};
} }
