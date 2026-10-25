#pragma once
#include <stdint.h>
namespace cielo96 { namespace selective {
constexpr uint32_t MIN_PENDING_MS=15000UL;
constexpr uint32_t MAX_PENDING_MS=300000UL;
// Conservative fail-open-to-global-rescue policy: only isolate an SDK cleanup
// that belongs exclusively to Chihiros while the single BLE worker is alive.
// Never touch BLE clients from this policy; a late SDK callback owns cleanup.
struct Input {
 bool ble_ready;
 bool ch_pending;
 bool pv_pending;
 uint32_t ch_pending_age_ms;
 uint32_t worker_heartbeat_age_ms;
 uint32_t worker_phase_age_ms;
 uint8_t worker_device;
 bool pv_off_required;
 bool ota_or_rescue;
};
inline bool hold_ch_only(const Input& x) {
 return x.ble_ready && x.ch_pending && !x.pv_pending &&
        x.ch_pending_age_ms>=MIN_PENDING_MS &&
        x.ch_pending_age_ms<MAX_PENDING_MS &&
        x.worker_heartbeat_age_ms<15000UL &&
        x.worker_phase_age_ms<15000UL &&
        !x.pv_off_required && !x.ota_or_rescue;
}
}} // namespace cielo96::selective
