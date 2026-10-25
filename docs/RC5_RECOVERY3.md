# RC5 RECOVERY3 · candidate based on working RC5 transport

## Evidence and comparison (25 October 2026, Madrid)

- SIMPLE1, source fa2eb67: report 64, boot 118, 17:56:36, generation 8 / PV34 / CH31. Owner confirms visible light/color control after rollback. Two successful PV operations and one CH operation; idle links released. This confirms that intentional BLE disconnection is compatible with continuing light output.
- RECOVERY1, report 62: PV physically unplugged while connected/CORRECTO persisted; success age 247 seconds. No new successful operations in that comparison.
- RECOVERY2, report 63: PV did not respond to CIELO despite successes two seconds old; one eight-second connection failure was detected, followed by transport successes. Owner could control it through the vendor app. NimBLE compatibility/root cause remains unproved; do not present RECOVERY2 as validated.
- Telegram screenshots confirm restart 116→117 at 17:31 and recovery at 17:34, then 117→118 at 17:50 and recovery at 17:53. The observer was receiving and sending. Recovery meant telemetry/transport, not light confirmation.

## Implementation

1. Returns to Arduino Bluedroid 2.0.17 used by SIMPLE1, retaining its discovery, service and protocol paths. No NimBLE dependency. Private pinned build copy, never modifies the installed SDK globally.
2. Bounded registration/open and write waits: registration max 2 s, total connection max 8 s, each write max 8 s. Scan remains 5 s. Service/descriptor discovery can still need the existing 120 s last-resort supervisor; no universal maximum for an entire operation is claimed.
3. Missing connection callbacks put that client in quarantine. No reuse, deletion or fake successful connection while the SDK request is pending. Late success is closed and cleanup allows retries. A missing cleanup callback for 120 s uses the existing supervisor budget and retained restart context; this is explicitly distinct from a frozen worker.
4. The write callback returns GATT errors, rather than discarding its status. Immediate SDK errors release the semaphore. Success remains transport-level only.
5. After a complete batch the BLE connection is intentionally released, leaving vendor apps free. Automatic control refreshes unchanged curves every 60 s. Positive PV consignments reassert POWER ON; zero brightness and the atomic OFF interlock preserve OFF priority, including during connection/writes. A successful night OFF is repeated every 60 s, consistent with the 90 s communication freshness limit.
6. Presence distinguishes current link, intentional idle release, pending cleanup, last successful send and its age. Recent means <=90 s and no consecutive error; an idle released link can have a recent send without being advertised as physically connected. UI says “Último envío BLE”, never infers light emission.
7. Keeps RECOVERY1 restart journal, first-restart observer notification, OFF priority, cadence diagnostics, host/origin LAN checks, profile slots, curves, codec and partitions. Existing HTTP/no password/OTA policy unchanged.
8. Sierra observer 4.0.1-SIERRA: preserves existing config, token/chat, database, outbox and LaunchAgent. New firmware publishes a 90 s communication limit; old firmware keeps its 650 s limit. Recovery wording describes recorded BLE sends and asks for a visual check. New ENVIAR_PRUEBA_TELEGRAM.command sends a real test only when invoked; PROBAR_TELEGRAM.command remains HTTPS-only. No automatic message is sent by the updater.

## Validation and remaining limits

Full host C++/JS/Python suites must pass, including actual patched functions, timeout/rollover/late callbacks, write status, private SDK preparation, OFF interlock, stale/released link UI, monitor compatibility and Sierra migration. CI compiles the ESP32-S3 binary against the pinned private Bluedroid library and checks stack override/size. Python 3.9 CI exercises the Sierra installer with simulated launchctl, not a physical Sierra machine.

Candidate has not yet run on the owner's ATOM. Before treating it as the functional replacement for SIMPLE1, verify visible lighting and color changes, unplug/replug recovery, vendor-app release, night OFF and boot OFF, followed by 24–72 h observation. Preserve the original SIMPLE1 binary for rollback. No host/CI success guarantees physical lamp response or Telegram reception.
