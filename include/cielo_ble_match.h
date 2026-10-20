#pragma once
#include <cstring>
// Identificacion BLE basada en datos observados en la controladora fisica.
// Pez Verde: nombre completo ELK-BLEDOB.
// Chihiros: el fabricante anuncia DYSL60 seguido de un identificador variable.
// Nunca usar prefijo generico para Pez Verde o buscar equipos por solo el UUID NUS.
namespace cielo96 {
inline bool bleNameMatches(const char* observed,const char* expected){
 if(!observed || !expected || !*observed || !*expected)return false;
 if(std::strcmp(expected,"DYSL60")==0)
  return std::strncmp(observed,"DYSL60",6)==0;
 return std::strcmp(observed,expected)==0;
}
}
