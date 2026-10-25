#pragma once
#include <stdint.h>
namespace cielo96 { namespace pvday {
// Daytime fast-path is permitted only AFTER the boot OFF transaction is
// GATT accepted, and only if current time/profile indicate brightness > 0.
// GATT is not optical proof. Unknown time must remain on safe boot OFF path.
struct Input {
 bool boot_gate,off_gatt,boot_healthy,clock_ok,profile_ok;
 bool ble_ready,ota_or_rescue,worker_alive;
 uint8_t current_brightness;
};
inline bool can_resume(const Input& x){
 return x.boot_gate && x.off_gatt && x.boot_healthy &&
  x.clock_ok && x.profile_ok && x.ble_ready &&
  !x.ota_or_rescue && x.worker_alive &&
  x.current_brightness>0;
}
inline bool defer_ch_return(bool pv_day_pending,uint32_t age_ms){
 return pv_day_pending && age_ms<20000UL;
}
}} // namespace cielo96::pvday
