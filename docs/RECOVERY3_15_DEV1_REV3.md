# Recovery 3.15 DEV1 REV3 — 26/11/2026
Base: DEV1 REV2 supplied ZIP. Development snapshot, not installable firmware.

- Restart journal: initial write deferred until normal 5-minute interval and headroom guard; low space/busy or failed write backs off 30 minutes. Pending evidence stays in RAM and can be lost on power failure. Preferences reports byte count, not exact ESP error; diagnostic explicitly reports this limitation. No NVS erase or migration.
- Independent latency classes feed global health warning. Legacy combined latency evaluation disabled; per-class baselines are authoritative. Missing fresh sample breaks consecutive streaks, not existing warning. Other trend metrics unchanged.
- Night plan reevaluated against current clock/profile/consigna after power boot. This is eligibility only, not a command. Experimental probe cancellation tested in pure policy; no BLE integration or optical validation. Do not enable automatically. No claim of fixing physical night recovery.
- Version string and CI artifact updated. No change to curves, queue, transport, UI, partitions or OTA policy.

Validation: host C++11 recovery315, health314, recovery314, ble_queue, night_safety, history314, recovery313; C++17 recovery315; extracted restart and diagnostic journal runtime fixtures; source contract. Target build and physical validation pending. DEV2 can branch from this snapshot for isolated development, but DEV1 is not release-qualified. Exact NVS error and active recovery remain open gates.
