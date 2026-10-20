#pragma once
#include <cstring>
#include <cstddef>
// Nombre BLE real de PV puede terminar con espacios de relleno (advertising).
// Solo ignoramos relleno ASCII final; no aceptamos prefijos PV ambiguos.
// Chihiros anuncia DYSL60 + identificador variable.
namespace cielo96 {
inline bool bleNameMatches(const char* observed,const char* expected){
 if(!observed || !expected || !*observed || !*expected)return false;
 const size_t expectedLen=std::strlen(expected);
 const size_t observedLen=std::strlen(observed);
 size_t trimmedLen=observedLen;
 while(trimmedLen>0 &&
       (observed[trimmedLen-1]==' ' || observed[trimmedLen-1]=='\t' ||
        observed[trimmedLen-1]=='\r' || observed[trimmedLen-1]=='\n'))trimmedLen--;
 if(std::strcmp(expected,"DYSL60")==0)
  return trimmedLen>=6 && std::strncmp(observed,"DYSL60",6)==0;
 return trimmedLen==expectedLen && std::strncmp(observed,expected,expectedLen)==0;
}
}
