#pragma once
// Contrato de severidad de CIELO: independiente de Arduino para tests host.
#include <stdint.h>
namespace cielo96 { namespace diag {
enum class Level : uint8_t { Ok, Error, Warning, NotTested, NoData };
inline const char* name(Level s){switch(s){case Level::Ok:return "CORRECTO";case Level::Error:return "FALLO";case Level::Warning:return "ADVERTENCIA";case Level::NotTested:return "NO_PROBADO";default:return "SIN_DATOS";}}
inline Level link(bool initialized,bool workerAlive,bool hasOperations,bool hasRecentSuccess,bool pending){
 if(!initialized)return Level::NoData;
 if(!workerAlive)return Level::Error;
 if(pending)return Level::Warning;
 if(!hasOperations)return Level::NotTested;
 return hasRecentSuccess?Level::Ok:Level::Warning;
}
// La fragmentación aquí solo expresa el porcentaje del heap libre que no cabe
// en un bloque contiguo; no presupone que exista fuga de memoria.
inline int fragmentationPct(uint32_t freeHeap,uint32_t largestBlock){
 if(!freeHeap)return -1;
 if(largestBlock>freeHeap)largestBlock=freeHeap;
 return static_cast<int>(100ULL*(freeHeap-largestBlock)/freeHeap);
}
inline bool canReportP95(uint32_t n){return n>=10;}
inline bool importantEvent(const char* event){
 if(!event)return false;
 // Solo codigos fijos, nunca detalles suministrados por la red.
 const char* patterns[]={"BLE_ERROR","BLE_SOFT_TIMEOUT","PV_POWER_OFF_REINTENTO",
 "PV_DEGRADADA","V4_WATCHDOG_PAUSA","REINICIO_ANOMALO","OTA_ERROR",
 "OTA_FIRMA_RECHAZADA","OTA_ROLLBACK","CURVA_ERROR","HTTPS_NO_DISPONIBLE",
 "BLE_PILOTO_ERROR","PV_NVS_FALLO","BLE_WORKER_RECUPERADO"};
 for(const char* pattern:patterns){const char*a=event;const char*b=pattern;
  while(*a&&*b&&*a==*b){++a;++b;} if(!*b&&(!*a||*a=='_'))return true;
 }
 return false;
}
inline bool shouldFlush(uint32_t now,uint32_t last,bool hasFlushed){
 return !hasFlushed || static_cast<uint32_t>(now-last)>=300000UL;
}
} }
