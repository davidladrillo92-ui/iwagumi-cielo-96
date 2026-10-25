#pragma once
#include <stdint.h>

namespace cielo96 { namespace blockage {

// Passive diagnostics only: no BLE calls, no task deletion, no restart policy change.
// A "discovery wait" is inferred from the worker's last phase; it does not prove
// which SDK callback was lost. An SDK pending request is independent of heartbeat.
enum class Kind : uint8_t {
  None=0, SdkConnectPending=1, GattDiscoveryWait=2, WorkerUnresponsive=3
};
struct Input {
  uint8_t worker_phase=0; // BleWorkerPhase: Service=5, Characteristic=6
  uint8_t worker_device=255;
  uint32_t worker_heartbeat_age_ms=0;
  uint32_t worker_phase_age_ms=0;
  uint32_t pending_pv_age_ms=0;
  uint32_t pending_ch_age_ms=0;
};
struct Result {
  Kind primary=Kind::None;
  bool sdk_pending=false;
  bool discovery_wait=false;
  bool worker_unresponsive=false;
  uint8_t pending_device=255;
  uint32_t pending_age_ms=0;
};
// 15 s is a diagnostic suspicion threshold, NOT the 120 s reboot threshold.
// In particular a pending SDK cleanup must not be reported as a dead worker.
inline Result classify(const Input& x, uint32_t suspect_ms=15000UL) {
  Result r{};
  r.pending_age_ms=x.pending_pv_age_ms;
  if(x.pending_pv_age_ms)r.pending_device=0;
  if(x.pending_ch_age_ms>r.pending_age_ms) {
    r.pending_age_ms=x.pending_ch_age_ms;r.pending_device=1;
  }
  r.sdk_pending=r.pending_device!=255;
  const bool stale=x.worker_heartbeat_age_ms>=suspect_ms &&
                   x.worker_phase_age_ms>=suspect_ms;
  r.discovery_wait=stale && (x.worker_phase==5 || x.worker_phase==6);
  r.worker_unresponsive=stale && !r.discovery_wait;
  if(r.discovery_wait)r.primary=Kind::GattDiscoveryWait;
  else if(r.worker_unresponsive)r.primary=Kind::WorkerUnresponsive;
  else if(r.sdk_pending)r.primary=Kind::SdkConnectPending;
  return r;
}
inline const char* name(Kind k) {
  switch(k) {
    case Kind::SdkConnectPending:return "SDK_CONNECT_PENDING";
    case Kind::GattDiscoveryWait:return "GATT_DISCOVERY_WAIT";
    case Kind::WorkerUnresponsive:return "WORKER_UNRESPONSIVE";
    default:return "NONE";
  }
}
}} // namespace cielo96::blockage
