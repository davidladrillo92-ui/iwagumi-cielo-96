#pragma once
#include <Arduino.h>
#include "cielo_ble_queue.h"
namespace cielo96 {
// Una sola tarea BLE propietaria del stack, aislada del servidor web.
struct BleResult {uint32_t sequence; bool success;uint32_t duration_ms;uint8_t device;uint8_t write_calls;char reason[50];};
bool ble_begin();
bool ble_dispatch(const cielo::ble::Command& c);
bool ble_result(BleResult& out);
uint32_t ble_last_heartbeat();
}
