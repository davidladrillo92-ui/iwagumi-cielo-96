#pragma once
#include <Arduino.h>
#include "cielo_ble_queue.h"
namespace cielo96 {
// Una sola tarea BLE propietaria del stack, aislada del servidor web.
struct BleResult {uint32_t sequence; bool success;uint32_t duration_ms;uint8_t device;uint8_t action;uint8_t write_calls;char reason[50];};
// Descubrimiento BLE activo (scan responses), sin conexiones ni escrituras GATT.
struct BleScanResult {
 uint32_t elapsed_ms=0; uint32_t completed_at_ms=0;
 bool pez_found=false, chihiros_found=false;
 int16_t pez_rssi=-127, chihiros_rssi=-127;
 uint16_t advertised=0, named=0, unnamed=0;
 struct NearbyDevice {char name[32]{};int16_t rssi=-127;bool nus=false;};
 NearbyDevice observed[8]{};
 uint8_t observed_count=0;
};
// Prueba de GATT: conecta, descubre servicio y escritura; NO escribe datos.
// Deja la conexión libre para My Chihiros / Pez Verde después del ensayo.
struct BleProbeResult {
 uint8_t device=0;
 uint32_t elapsed_ms=0;
 bool success=false,service=false,characteristic=false;
 char reason[48]{};
};
bool ble_probe_request(uint8_t device);
bool ble_probe_result(BleProbeResult& result);
bool ble_scan_request();
bool ble_scan_result(BleScanResult& out);
bool ble_begin();
bool ble_dispatch(const cielo::ble::Command& c);
bool ble_result(BleResult& out);
uint32_t ble_last_heartbeat();
}
