#pragma once
// B3: contratos de caducidad de sesiones y defensa de origen/CSRF.
// Puro C++ para poder probarlo fuera del ESP32.
#include <stdint.h>
#include <string.h>
namespace cielo96 { namespace security {
static constexpr uint32_t idleMs=900000UL;
static constexpr uint32_t absoluteMs=7200000UL;
inline bool sessionFresh(bool active,uint32_t now,uint32_t issued,uint32_t last){
 return active && static_cast<uint32_t>(now-last)<=idleMs &&
        static_cast<uint32_t>(now-issued)<=absoluteMs;
}
inline bool retryBefore(uint32_t now,uint32_t deadline){
 return deadline && static_cast<int32_t>(now-deadline)<0;
}
inline bool safeHost(const char*host){
 if(!host)return false;
 const size_t n=strlen(host);
 if(n<1||n>128)return false;
 for(size_t i=0;i<n;i++){
  char c=host[i];
  if(!(c=='-'||c=='.'||c==':'||c=='['||c==']'||
       (c>='0'&&c<='9')||(c>='A'&&c<='Z')||(c>='a'&&c<='z')))
   return false;
 }
 return true;
}
inline bool sameOrigin(const char*origin,const char*host){
 if(!safeHost(host))return false;
 if(!origin||!*origin)return true; // clientes locales sin Origin usan token
 static constexpr char scheme[]="https://";
 if(strncmp(origin,scheme,sizeof(scheme)-1)!=0)return false;
 return strcmp(origin+sizeof(scheme)-1,host)==0;
}
inline bool sameLanOrigin(const char*origin,const char*host){
 if(!safeHost(host))return false;
 if(!origin||!*origin)return true;
 static constexpr char scheme[]="http://";
 return strncmp(origin,scheme,sizeof(scheme)-1)==0 &&
        strcmp(origin+sizeof(scheme)-1,host)==0;
}
inline bool safeFetchSite(const char*site){
 return !site||!*site||strcmp(site,"same-origin")==0||strcmp(site,"none")==0;
}
// Restrict the Host as well as Origin; blocks DNS rebinding without passwords
// or TLS allocations. Only the device's addresses and its mDNS name are served.
inline bool localHost(const char*host,const char*sta,const char*ap){
 if(!safeHost(host))return false;
 char normalized[129]{};size_t n=strlen(host);
 if(n>3 && strcmp(host+n-3,":80")==0)n-=3;
 for(size_t i=0;i<n;i++)normalized[i]=(host[i]>='A'&&host[i]<='Z')?host[i]+('a'-'A'):host[i];
 return strcmp(normalized,"iwagumi-cielo96.local")==0 ||
        (sta&&*sta&&strcmp(sta,"0.0.0.0")!=0&&strcmp(normalized,sta)==0) ||
        (ap&&*ap&&strcmp(normalized,ap)==0);
}
} }
