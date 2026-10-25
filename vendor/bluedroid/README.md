# CIELO private Bluedroid build copy

Upstream: Arduino-ESP32 **2.0.17**, https://github.com/espressif/arduino-esp32/tree/2.0.17/libraries/BLE . Original authorship notices are retained. `upstream/` contains exact reference files and `UPSTREAM_SHA256.json` pins them.

`tools/prepare_bluedroid.py` checks the installed SDK files, copies its entire BLE library into a project-private ignored folder, and overlays these four changed files. The SDK itself remains untouched. PlatformIO compiles `CieloBluedroid` and excludes the unmodified `ESP32 BLE Arduino` library.

Changes to BLEClient: two-second registration and eight-second total registration/open deadline (scan is separate); nonblocking semaphore acquisition; a timed-out client cannot be reused until the original SDK event settles; late successes are closed rather than promoted; REG events filter app_id. Two stable clients are retained; no client/task deletion. If SDK cleanup never arrives, the application retains a last-resort supervised full restart at 120 seconds, with the existing budget.

Changes to characteristic write: the old void API remains compatible; new `cieloWriteValue` returns SDK/GATT status with eight-second deadline and releases the semaphore on immediate SDK errors. The callback preserves the actual GATT status. Payload, write type and protocol order are unchanged. Success never proves light emission.

`tests/test_rc5_bluedroid_runtime.py` executes the actual patched connect/write functions and late-event guard with a 10 ms tick simulation, deadlines across rollover, foreign/late events, quarantine/reuse and SDK/GATT failure. It also verifies private build preparation and rejects unexpected SDK source bytes.
