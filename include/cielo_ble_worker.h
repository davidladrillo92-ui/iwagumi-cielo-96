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
// Ceder el control al telefono sin modificar RGB ni horarios.
// La desconexion es propiedad exclusiva del worker BLE.
struct BleLinkStatus {
 bool connected=false;
 bool released_to_app=false;
 uint32_t release_count=0;
 uint32_t last_disconnect_ms=0;
};
// Experimento exclusivo: select custom curve (0x03), GATT en worker y desconectar.
struct BleCustomResumeResult {bool success=false;uint32_t duration_ms=0;char reason[48]{};};
bool ble_custom_resume_request();
bool ble_custom_resume_result(BleCustomResumeResult& result);
bool ble_release_request(uint8_t device);
BleLinkStatus ble_link_status(uint8_t device);
bool ble_probe_request(uint8_t device);
bool ble_probe_result(BleProbeResult& result);
bool ble_scan_request();
bool ble_scan_result(BleScanResult& out);
bool ble_begin();
bool ble_dispatch(const cielo::ble::Command& c);
bool ble_result(BleResult& out);
uint32_t ble_last_heartbeat();
// Telemetria de fases: lectura pasiva; nunca toca Bluetooth fuera de su tarea.
enum class BleWorkerPhase : uint8_t {
 Starting=1, Idle=2, Discovery=3, Connecting=4, Service=5,
 Characteristic=6, Writing=7, Disconnecting=8, Resuming=9,
 Probing=10, Sending=11
};
// Contadores de transporte de la tarea BLE; duración de cada fase, no ACK óptico.
struct BleTransportStats {
 uint32_t connect_count=0,connect_failed=0,connect_last_ms=0,connect_max_ms=0;
 uint32_t write_count=0,write_failed=0,write_last_ms=0,write_max_ms=0;
 uint32_t disconnect_count=0,disconnect_last_ms=0,disconnect_max_ms=0;
};
BleTransportStats ble_transport_stats(uint8_t device);
uint32_t ble_request_queue_depth();
struct BleWorkerSnapshot {
 uint8_t phase=0,device=255;
 uint32_t phase_age_ms=0,heartbeat_age_ms=0;
 bool in_operation=false;
};
BleWorkerSnapshot ble_worker_snapshot();
const char* ble_worker_phase_name(uint8_t phase);
}
