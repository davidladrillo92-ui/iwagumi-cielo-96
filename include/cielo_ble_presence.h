#pragma once
#include <stdint.h>
namespace cielo96 { namespace presence {
// Active curve refresh also verifies the transport when the RGB does not change.
constexpr uint32_t REFRESH_MS=60000UL, FRESH_MS=90000UL;
inline bool refreshDue(uint32_t now,uint32_t last){return uint32_t(now-last)>=REFRESH_MS;}
inline bool recent(bool connected,uint32_t lastOk,uint32_t now,uint32_t errors){
 return connected && lastOk!=0 && errors==0 && uint32_t(now-lastOk)<=FRESH_MS;
}
// Planned OFF is a known past command awaiting its scheduled check, not live availability.
constexpr uint32_t OFF_CHECK_MS=300000UL;
inline bool plannedOff(bool connected,bool offConfirmed,uint32_t lastOff,
                       uint32_t lastSuccess,uint32_t now,uint32_t errors){
 return connected && offConfirmed && !errors && lastOff!=0 && lastOff==lastSuccess &&
        uint32_t(now-lastOff)<OFF_CHECK_MS;
}
} }
