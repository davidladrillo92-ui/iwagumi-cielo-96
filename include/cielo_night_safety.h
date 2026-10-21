#pragma once
// RC5 B1: politica de apagado puro C++17, reutilizada por el ESP32 y test host.
// Ningun ACK GATT permite inferir que la lampara esta apagada opticamente.
#include <stdint.h>
namespace cielo96 {
namespace safety {
inline bool offRequired(bool controlEnabled,bool validClock,bool validProfile,uint8_t brightness){
 return controlEnabled&&validClock&&validProfile&&brightness==0;
}
inline bool forbidStaleRgb(bool offObligation,uint8_t brightness){
 return offObligation&&brightness!=0;
}
inline uint32_t offRetryDelayMs(uint32_t consecutiveFailures){
 return consecutiveFailures<2?5000UL:(consecutiveFailures<4?15000UL:60000UL);
}
inline bool isOffAck(bool offObligation,uint32_t offInFlight,uint32_t resultSeq){
 return offObligation&&offInFlight!=0&&offInFlight==resultSeq;
}
inline bool retryDue(uint32_t now,uint32_t due){
 return static_cast<int32_t>(now-due)>=0;
}
}
}
