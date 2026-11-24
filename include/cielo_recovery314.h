#pragma once
#include <stdint.h>

namespace cielo96 { namespace r314 {

// Safety is independent of the 180 s OTA observation. No colour may be
// scheduled without a fresh trusted clock, a valid profile, and a live BLE task.
struct BootResumeInput {
 uint32_t elapsed_ms;
 bool clock_trusted;
 bool profile_valid;
 bool ble_ready;
 bool coexist_ready;
 bool worker_alive;
 bool ota_busy;
 bool rescue;
 bool watchdog;
 bool maintenance;
 bool queue_busy;
 bool ch_return_pending;
 bool pv_control_desired;
 bool pv_boot_off_gate;
 bool pv_off_gatt;
 bool pv_off_required;
 bool current_pv_day;
};
inline bool may_resume_curves(const BootResumeInput& x) {
 if(x.elapsed_ms<45000UL || !x.clock_trusted || !x.profile_valid ||
    !x.ble_ready || !x.coexist_ready || !x.worker_alive ||
    x.ota_busy || x.rescue || x.watchdog || x.maintenance ||
    x.queue_busy || x.ch_return_pending)return false;
 // Cold boot always sends PV OFF before any application colour.
 if(x.pv_control_desired) {
  if(!x.pv_off_gatt && (x.pv_boot_off_gate || x.pv_off_required))return false;
  // Daylight RGB requires the separate PV recovery interlock to have cleared.
  if(x.current_pv_day && (x.pv_boot_off_gate || x.pv_off_required))return false;
  // At night CH can start after PV OFF; PV must remain OFF.
  if(!x.current_pv_day && x.pv_off_required && !x.pv_off_gatt)return false;
 }
 return true;
}
// BLE's radio needs a protected AP, not a WAN or joined home router.
inline bool may_start_ble(uint32_t elapsed_ms,bool ap_ready,bool coexist_ready,
 bool rescue,bool attempted,bool retry_window_ready) {
 return elapsed_ms>=30000UL && ap_ready && coexist_ready &&
   !rescue && !attempted && retry_window_ready;
}
inline bool may_recover_day_pv(uint32_t elapsed_ms,bool trusted_clock,
 bool valid_profile,bool ble_ready,bool coexist_ready,bool worker_alive,
 bool off_gatt,bool daytime,bool ota_busy,bool rescue) {
 return elapsed_ms>=45000UL && trusted_clock && valid_profile && ble_ready &&
  coexist_ready && worker_alive && off_gatt && daytime && !ota_busy && !rescue;
}
inline bool prioritize_pv_day(uint32_t elapsed_ms,bool gate,bool off_gatt,
 bool daytime,bool trusted_clock) {
 return elapsed_ms<90000UL && gate && off_gatt && daytime && trusted_clock;
}
// From RTC_NOINIT, only an uncontaminated software restart can continue a
// recently trusted clock. Power cycle/brownout/panic require NTP/phone.
inline bool trusted_rtc_continuity(bool software_restart,uint32_t rtc_epoch,
 uint32_t now_epoch,bool checksum_valid) {
 return software_restart && checksum_valid && rtc_epoch>1700000000UL &&
   now_epoch>=rtc_epoch && uint32_t(now_epoch-rtc_epoch)<=900UL;
}
inline bool matching_return(uint32_t expected,uint32_t observed,bool pending) {
 return pending && expected!=0 && observed==expected;
}
inline bool cold_discovery_slow(uint32_t elapsed_ms,uint32_t discovery_ms) {
 return elapsed_ms>3500UL && discovery_ms>=3000UL;
}
}} // namespace cielo96::r314
