#pragma once
#include <stdint.h>
#include <stddef.h>
#include "cielo_nvs_safe.h"
namespace cielo96 { namespace r313 {
// Keep one NVS page for GC, 12 entries for restart/control metadata,
// and 384 bytes of journal budget. The SDK remains authoritative.
inline size_t diagnosticReserve(bool littlefs,size_t curveBytes){
 return littlefs?126u+12u+nvssafe::minimumFreeEntries(384):nvssafe::diagnosticReserve(curveBytes);
}
inline bool olderThanKnown(uint32_t generation,uint32_t known){
 return known!=0 && (generation==0 || (generation!=known && uint32_t(generation-known)>=0x80000000UL));
}
inline uint64_t elapsedMs(uint64_t bootUs,uint64_t nowUs){return nowUs>=bootUs?(nowUs-bootUs)/1000ULL:0;}
inline bool lampFault(bool expected,bool active,bool fresh,bool inhibited){
 return expected&&!inhibited&&(!active||!fresh);
}
} }
