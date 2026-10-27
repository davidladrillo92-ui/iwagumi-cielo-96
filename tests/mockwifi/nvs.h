#pragma once
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <cassert>
using nvs_handle_t=uint32_t;
constexpr int ESP_OK=0,ESP_ERR_NVS_NOT_FOUND=1,ESP_ERR_NVS_NOT_ENOUGH_SPACE=2,ESP_ERR_NVS_INVALID_HANDLE=3;
constexpr int NVS_READONLY=0,NVS_READWRITE=1;
namespace mocknvs {
inline std::map<std::string,std::vector<uint8_t>> data;
inline std::map<nvs_handle_t,int> handles;
inline nvs_handle_t next=1;
inline int opens=0,closes=0,reads=0,writes=0,commits=0;
inline int readFailAt=0,openError=0,writeError=0,commitError=0,tornBytes=-1;
inline bool failAfterWrite=false,mutateVerify=false;
inline nvs_handle_t lastWrite=0;
inline void reset(){data.clear();handles.clear();next=1;opens=closes=reads=writes=commits=0;readFailAt=openError=writeError=commitError=0;tornBytes=-1;failAfterWrite=mutateVerify=false;lastWrite=0;}
}
inline int nvs_open(const char*ns,int mode,nvs_handle_t*out){
 assert(std::string(ns)=="cielo96");if(mocknvs::openError)return mocknvs::openError;
 *out=mocknvs::next++;mocknvs::handles[*out]=mode;mocknvs::opens++;return ESP_OK;
}
inline int nvs_get_blob(nvs_handle_t h,const char*k,void*out,size_t*size){
 assert(mocknvs::handles.count(h));assert(h!=mocknvs::lastWrite); // Independent read handle required.
 mocknvs::reads++;if(mocknvs::readFailAt==mocknvs::reads)return -100;
 auto it=mocknvs::data.find(k);if(it==mocknvs::data.end())return ESP_ERR_NVS_NOT_FOUND;
 if(!out){*size=it->second.size();return ESP_OK;}
 if(*size<it->second.size())return -101;
 *size=it->second.size();memcpy(out,it->second.data(),*size);
 if(mocknvs::mutateVerify&&mocknvs::writes)static_cast<uint8_t*>(out)[30]^=1;
 return ESP_OK;
}
inline int nvs_set_blob(nvs_handle_t h,const char*k,const void*in,size_t size){
 assert(mocknvs::handles.at(h)==NVS_READWRITE);assert(std::string(k)=="wifiA"||std::string(k)=="wifiB");assert(size==112);
 mocknvs::writes++;mocknvs::lastWrite=h;
 if(mocknvs::writeError&&!mocknvs::failAfterWrite)return mocknvs::writeError;
 const auto*p=static_cast<const uint8_t*>(in);mocknvs::data[k]=std::vector<uint8_t>(p,p+size);
 if(mocknvs::tornBytes>=0){for(size_t i=size_t(mocknvs::tornBytes);i<size;i++)mocknvs::data[k][i]=0xa5;return -102;}
 return mocknvs::writeError;
}
inline int nvs_commit(nvs_handle_t h){assert(mocknvs::handles.at(h)==NVS_READWRITE);mocknvs::commits++;return mocknvs::commitError;}
inline void nvs_close(nvs_handle_t h){assert(mocknvs::handles.erase(h)==1);mocknvs::closes++;}
// No erase/format functions are declared: accidental calls fail compilation.
