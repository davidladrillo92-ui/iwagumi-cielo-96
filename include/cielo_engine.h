#pragma once
#include <stddef.h>
#include <stdint.h>
namespace cielo {
struct PvPoint { uint16_t minute; uint8_t r,g,b,brightness; };
struct ChiPoint { uint16_t minute; uint8_t r,g,b; };
struct PvValue { uint8_t r,g,b,brightness; };
struct ChiValue { uint8_t r,g,b; };
// The exported schedules express MINUTES SINCE MIDNIGHT (0..1440).
// Internal clock and curve simulation are intentionally NOT shared.
inline uint8_t mix(uint8_t a,uint8_t b,uint32_t position,uint32_t interval) {
  if(interval==0) return b;
  const int32_t delta=int32_t(b)-int32_t(a);
  const int32_t offset=(delta>=0)
    ? (delta*int32_t(position)+int32_t(interval/2))/int32_t(interval)
    : -((-delta*int32_t(position)+int32_t(interval/2))/int32_t(interval));
  const int32_t out=int32_t(a)+offset;
  return uint8_t(out<0?0:(out>255?255:out));
}
inline bool valid(const PvPoint* p,size_t n) {
 if(!p || n<2 || n>64) return false;
 for(size_t i=0;i<n;i++){
   if(p[i].minute>1440 || p[i].brightness>100 || (i && p[i].minute<=p[i-1].minute))return false;
 }
 return true;
}
inline bool valid(const ChiPoint* p,size_t n) {
 if(!p || n<2 || n>64) return false;
 for(size_t i=0;i<n;i++){
   if(p[i].minute>1440 || (i && p[i].minute<=p[i-1].minute))return false;
 }
 return true;
}
inline PvValue at(const PvPoint* p,size_t n,uint16_t minute){
 if(!valid(p,n) || minute>1440) return {0,0,0,0};
 if(minute<p[0].minute) return {0,0,0,0};
 if(minute>=p[n-1].minute)return {p[n-1].r,p[n-1].g,p[n-1].b,p[n-1].brightness};
 size_t lo=0,hi=n-1;
 while(hi-lo>1){size_t mid=(lo+hi)/2;if(p[mid].minute<=minute)lo=mid;else hi=mid;}
 auto &a=p[lo],&b=p[hi];uint32_t d=uint32_t(b.minute-a.minute),t=uint32_t(minute-a.minute);
 return {mix(a.r,b.r,t,d),mix(a.g,b.g,t,d),mix(a.b,b.b,t,d),mix(a.brightness,b.brightness,t,d)};
}
inline ChiValue at(const ChiPoint* p,size_t n,uint16_t minute){
 if(!valid(p,n) || minute>1440) return {0,0,0};
 if(minute<p[0].minute) return {0,0,0};
 if(minute>=p[n-1].minute)return {p[n-1].r,p[n-1].g,p[n-1].b};
 size_t lo=0,hi=n-1;
 while(hi-lo>1){size_t mid=(lo+hi)/2;if(p[mid].minute<=minute)lo=mid;else hi=mid;}
 auto &a=p[lo],&b=p[hi];uint32_t d=uint32_t(b.minute-a.minute),t=uint32_t(minute-a.minute);
 return {mix(a.r,b.r,t,d),mix(a.g,b.g,t,d),mix(a.b,b.b,t,d)};
}
} // namespace cielo
