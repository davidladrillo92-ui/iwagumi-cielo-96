#pragma once
#include <stdint.h>
#include <stddef.h>
namespace cielo96 { namespace ota {
static constexpr size_t prefixBytes=288;
inline bool compatiblePrefix(const uint8_t*p,size_t n){
 if(!p||n<prefixBytes)return false;
 // ESP image + S3 chip id + 8MiB flash + first segment app descriptor.
 return p[0]==0xe9 && p[1]>0 && p[1]<=16 && p[12]==9 && p[13]==0 &&
  (p[3]>>4)==3 && p[32]==0x32 && p[33]==0x54 && p[34]==0xcd && p[35]==0xab;
}
struct HealthyWindow {
 bool active=false;uint32_t since=0;
 bool tick(uint32_t now,bool healthy){
  if(!healthy){active=false;return false;}
  if(!active){active=true;since=now;return false;}
  return static_cast<uint32_t>(now-since)>=15000UL;
 }
};
} }
