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
} }
