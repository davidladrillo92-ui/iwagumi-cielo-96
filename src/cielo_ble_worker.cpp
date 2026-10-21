#include "cielo_ble_worker.h"
#include "cielo_codec.h"
#include "cielo_ble_match.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEClient.h>
#include <BLERemoteCharacteristic.h>
#include <string.h>

namespace cielo96 {
namespace {
struct Target {
 Target(const char* n, const char* s, const char* w): name(n), service(s), writeUuid(w) {}
 const char* name; const char* service; const char* writeUuid;
 bool addressKnown=false;
 String address;
 esp_ble_addr_type_t addressType=BLE_ADDR_TYPE_PUBLIC;
 BLEClient* client=nullptr;
 BLERemoteCharacteristic* writer=nullptr;
 bool manualEntered=false;
 bool pvOn=false;
};
Target targets[2]={{codec::PV_NAME,codec::PV_SVC,codec::PV_TX},
                   {codec::CH_NAME,codec::CH_SVC,codec::CH_TX}};
QueueHandle_t requests=nullptr,responses=nullptr,scanRequests=nullptr,scanResponses=nullptr,probeRequests=nullptr,probeResponses=nullptr,releaseRequests=nullptr,customRequests=nullptr,customResults=nullptr;
struct SharedLink { volatile bool connected=false;volatile bool released=false;volatile uint32_t releases=0;volatile uint32_t lastDisconnect=0;};
SharedLink shared[2]{};
uint32_t linkTouched[2]{};
static constexpr uint32_t BLE_IDLE_RELEASE_MS=5000UL;
volatile uint32_t lastHeartbeat=0;
volatile uint8_t workerPhase=static_cast<uint8_t>(BleWorkerPhase::Starting);
volatile uint8_t workerDevice=255;
volatile uint32_t phaseEnteredAt=0;
void setWorkerPhase(BleWorkerPhase p,uint8_t dev=255){
 workerDevice=dev;phaseEnteredAt=millis();workerPhase=static_cast<uint8_t>(p);
}
void releaseTarget(uint8_t device,bool userRequested){
 setWorkerPhase(BleWorkerPhase::Disconnecting,device);
 Target& t=targets[device];
 setWorkerPhase(BleWorkerPhase::Disconnecting,device);
 if(t.client && t.client->isConnected())t.client->disconnect();
 t.writer=nullptr;t.manualEntered=false;t.pvOn=false;
 // Mantener direccion/tipo aprendidos para evitar scan de 5s en cada orden;
 // ante un fallo real de conexion se invalida en connect().
 shared[device].connected=false;
 shared[device].released=userRequested;
 shared[device].releases++;
 shared[device].lastDisconnect=millis();
 linkTouched[device]=0;
}
void releaseIdleLinks(){
 uint32_t now=millis();
 for(uint8_t k=0;k<2;k++)
  if(linkTouched[k] && now-linkTouched[k]>=BLE_IDLE_RELEASE_MS)
   releaseTarget(k,false);
}
uint16_t chMessageId=1;
uint8_t writesInCurrentOperation=0;
// Exactly one FreeRTOS task owns all BLE calls. Web thread NEVER interacts with the stack.
void copyReason(BleResult& r,const char* msg){strncpy(r.reason,msg,sizeof(r.reason)-1);r.reason[sizeof(r.reason)-1]='\0';}
bool locate(Target& t) {
 if(t.addressKnown) return true;
 BLEScan* scan=BLEDevice::getScan();
 scan->setActiveScan(true);
 scan->setInterval(150);scan->setWindow(75);
 setWorkerPhase(BleWorkerPhase::Discovery,&t==&targets[0]?0:1);
 BLEScanResults result=scan->start(5,false);
 bool found=false;
 for(int i=0;i<result.getCount();++i){
  BLEAdvertisedDevice d=result.getDevice(i);
  if(d.haveName() && bleNameMatches(d.getName().c_str(),t.name)){
   t.address=String(d.getAddress().toString().c_str());
   t.addressType=d.getAddressType();t.addressKnown=true;found=true;break;
  }
 }
 scan->clearResults();
 return found;
}
bool connect(Target& t,const char*&err){
 if(t.client && t.client->isConnected() && t.writer){
  shared[&t==&targets[0]?0:1].connected=true;
  return true;
 }
 t.writer=nullptr;t.manualEntered=false;
 if(!locate(t)){err="BLE_DEVICE_NOT_FOUND";return false;}
 if(!t.client)t.client=BLEDevice::createClient();
 if(!t.client){err="BLE_CLIENT_CREATE";return false;}
 // connect(BLEAddress) on Arduino-ESP32 2.x; this may block, isolated from HTTP.
 BLEAddress address(std::string(t.address.c_str()));
 setWorkerPhase(BleWorkerPhase::Connecting,&t==&targets[0]?0:1);
 if(!t.client->connect(address,t.addressType)){
  err="BLE_CONNECT_FAILED";t.addressKnown=false;return false;
 }
 setWorkerPhase(BleWorkerPhase::Service,&t==&targets[0]?0:1);
 BLERemoteService* service=t.client->getService(BLEUUID(t.service));
 if(!service){err="GATT_SERVICE_MISSING";t.client->disconnect();return false;}
 setWorkerPhase(BleWorkerPhase::Characteristic,&t==&targets[0]?0:1);
 t.writer=service->getCharacteristic(BLEUUID(t.writeUuid));
 if(!t.writer || !(t.writer->canWrite()||t.writer->canWriteNoResponse())){
  err="GATT_CHARACTERISTIC_MISSING";t.client->disconnect();t.writer=nullptr;return false;
 }
 shared[&t==&targets[0]?0:1].connected=true;
 shared[&t==&targets[0]?0:1].released=false;
 return true;
}
bool put(Target& t,const uint8_t* data,size_t n,const char*&err){
 if(!t.client || !t.client->isConnected() || !t.writer){err="GATT_NOT_CONNECTED";return false;}
 // Arduino BLE writeValue returns void: transport accepted != optical ACK!
 setWorkerPhase(BleWorkerPhase::Writing,&t==&targets[0]?0:1);
 t.writer->writeValue(const_cast<uint8_t*>(data),n,t.writer->canWrite());
 if(writesInCurrentOperation<255)writesInCurrentOperation++; // GATT write call, not optical acknowledgement
 if(!t.client->isConnected()){err="BLE_DISCONNECTED_DURING_WRITE";t.writer=nullptr;return false;}
 vTaskDelay(pdMS_TO_TICKS(30));
 return true;
}
bool sendPez(const cielo::ble::Command& c,const char*&err){
 Target&t=targets[0];if(!connect(t,err))return false;
 uint8_t frame[9];
 if(c.brightness==0){codec::pv_power(false,frame);if(!put(t,frame,9,err))return false;t.pvOn=false;return true;}
 if(!t.pvOn){codec::pv_power(true,frame);if(!put(t,frame,9,err))return false;t.pvOn=true;}
 codec::pv_color(c.r,c.g,c.b,frame);if(!put(t,frame,9,err))return false;
 codec::pv_brightness(c.brightness,frame);return put(t,frame,9,err);
}
bool sendCh(const cielo::ble::Command& c,const char*&err){
 Target&t=targets[1];if(!connect(t,err))return false;
 uint8_t frame[32];size_t size=0;
 if(c.action==cielo::ble::Action::RestoreAuto){
  // Fallo fisico Informe Maestro 22: la trama 0x05 [18,ff,ff]
  // selecciono escenas y apago la Chihiros con curva personalizada.
  // Defensa en profundidad: incluso si se encolase, NO escribir GATT.
  err="CH_AUTO_MODE_UNVERIFIED";
  return false;
 }
 if(!t.manualEntered){
  if(!codec::ch_manual_prelude(chMessageId,frame,sizeof(frame),size)){err="CH_PRELUDE_ENCODE";return false;}
  if(!put(t,frame,size,err))return false;t.manualEntered=true;
 }
 const uint8_t levels[3]={c.r,c.g,c.b};
 for(uint8_t channel=0;channel<3;channel++){
  if(!codec::ch_manual(channel,levels[channel],chMessageId,frame,sizeof(frame),size)){
   err="CH_RGB_ENCODE";return false;
  }
  if(!put(t,frame,size,err))return false;
 }
 return true;
}
void scanDiscover(){
 BleScanResult r{};
 uint32_t start=millis();
 BLEScan* scan=BLEDevice::getScan();
 // Active scan requests scan-response packets where controllers often advertise their full name.
 // This remains discovery-only: does not connect or write a single GATT byte.
 scan->setActiveScan(true);
 scan->setInterval(160);
 scan->setWindow(80);
 setWorkerPhase(BleWorkerPhase::Discovery);
 BLEScanResults found=scan->start(5,false);
 r.advertised=found.getCount();
 for(int i=0;i<found.getCount();++i){
  BLEAdvertisedDevice d=found.getDevice(i);
  const std::string name=d.haveName()?d.getName():std::string();
  if(name.empty())r.unnamed++;else r.named++;
  if(r.observed_count<8){
   auto& rec=r.observed[r.observed_count++];
   const char* label=name.empty()?"(sin nombre)":name.c_str();
   strncpy(rec.name,label,sizeof(rec.name)-1);rec.name[sizeof(rec.name)-1]='\0';
   rec.rssi=d.getRSSI();
   rec.nus=d.isAdvertisingService(BLEUUID(codec::CH_SVC));
  }
  if(name.empty())continue;
  if(bleNameMatches(name.c_str(),codec::PV_NAME)){r.pez_found=true;r.pez_rssi=d.getRSSI();}
  if(bleNameMatches(name.c_str(),codec::CH_NAME)){r.chihiros_found=true;r.chihiros_rssi=d.getRSSI();}
 }
 scan->clearResults();
 r.elapsed_ms=millis()-start;
 r.completed_at_ms=millis();
 if(xQueueSend(scanResponses,&r,0)!=pdTRUE){
  BleScanResult discarded{};xQueueReceive(scanResponses,&discarded,0);xQueueSend(scanResponses,&r,0);
 }
}
void probeGATT(uint8_t device){
 BleProbeResult r{};r.device=device;const uint32_t start=millis();
 Target& t=targets[device==1?1:0];const char* reason="OK";
 setWorkerPhase(BleWorkerPhase::Probing,device);
 // Si CIELO conserva una conexion anterior de prueba manual, soltarla antes
 // de realizar la sonda para no iniciar otra conexion encima.
 if(t.client && t.client->isConnected())t.client->disconnect();
 t.writer=nullptr;t.manualEntered=false;
 if(!locate(t)){
  reason="BLE_DEVICE_NOT_FOUND";
 }else{
  if(!t.client)t.client=BLEDevice::createClient();
  if(!t.client){reason="BLE_CLIENT_CREATE";}
  else {
   BLEAddress address(std::string(t.address.c_str()));
   setWorkerPhase(BleWorkerPhase::Connecting,device);
   if(!t.client->connect(address,t.addressType)){
    reason="BLE_CONNECT_FAILED";
   }else{
    setWorkerPhase(BleWorkerPhase::Service,device);
    BLERemoteService* svc=t.client->getService(BLEUUID(t.service));
    if(!svc)reason="GATT_SERVICE_MISSING";
    else {
     r.service=true;
     setWorkerPhase(BleWorkerPhase::Characteristic,device);
     BLERemoteCharacteristic* characteristic=svc->getCharacteristic(BLEUUID(t.writeUuid));
     if(!characteristic || !(characteristic->canWrite()||characteristic->canWriteNoResponse())){
      reason="GATT_CHARACTERISTIC_MISSING";
     }else{
      r.characteristic=true;r.success=true;
     }
    }
   }
  }
 }
 // Se suelta siempre la conexion; ni escritura ni suscripcion de notificaciones.
 releaseTarget(device,false);
 strncpy(r.reason,reason,sizeof(r.reason)-1);
 r.elapsed_ms=millis()-start;
 if(xQueueSend(probeResponses,&r,0)!=pdTRUE){
  BleProbeResult old{};xQueueReceive(probeResponses,&old,0);xQueueSend(probeResponses,&r,0);
 }
}
void resumeCustomCurve(){
 BleCustomResumeResult res{};uint32_t started=millis();const char*err="OK";
 setWorkerPhase(BleWorkerPhase::Resuming,1);
 Target& t=targets[1];uint8_t frame[32];size_t n=0;
 const bool connected=connect(t,err);
 if(connected){
  if(!codec::ch_custom_curve_resume(chMessageId,frame,sizeof(frame),n)){
   err="CH_CUSTOM_FRAME";res.success=false;
  }else{
   res.success=put(t,frame,n,err);
  }
 }
 res.duration_ms=millis()-started;
 strncpy(res.reason,err,sizeof(res.reason)-1);
 releaseTarget(1,true); // Nunca quedar bloqueando My Chihiros.
 if(xQueueSend(customResults,&res,0)!=pdTRUE){
  BleCustomResumeResult old{};xQueueReceive(customResults,&old,0);xQueueSend(customResults,&res,0);
 }
}
void task(void*){
 setWorkerPhase(BleWorkerPhase::Starting);
 BLEDevice::init("IWAGUMI-CIELO-96");
 lastHeartbeat=millis();
 for(;;){
  setWorkerPhase(BleWorkerPhase::Idle);
  lastHeartbeat=millis();
  uint8_t probeToken=0;
  uint8_t releaseToken=0;
  while(xQueueReceive(releaseRequests,&releaseToken,0)==pdTRUE){
   if(releaseToken<2)releaseTarget(releaseToken,true);
  }
  releaseIdleLinks();
  uint8_t customToken=0;
  if(xQueueReceive(customRequests,&customToken,0)==pdTRUE){
   resumeCustomCurve();lastHeartbeat=millis();
  }
  if(xQueueReceive(probeRequests,&probeToken,0)==pdTRUE){
   probeGATT(probeToken);lastHeartbeat=millis();
  }
  uint8_t scanToken=0;
  if(xQueueReceive(scanRequests,&scanToken,0)==pdTRUE){
   scanDiscover();lastHeartbeat=millis();
  }
  cielo::ble::Command c{};
  // Block without consuming CPU; heartbeat updates every second when idle.
  if(xQueueReceive(requests,&c,pdMS_TO_TICKS(1000))!=pdTRUE){lastHeartbeat=millis();continue;}
  setWorkerPhase(BleWorkerPhase::Sending,static_cast<uint8_t>(c.device));
  uint32_t started=millis();writesInCurrentOperation=0;const char*err="OK";
  bool success=(c.device==cielo::ble::Device::PezVerde)?sendPez(c,err):sendCh(c,err);
  BleResult res{};res.sequence=c.sequence;res.device=static_cast<uint8_t>(c.device);res.action=static_cast<uint8_t>(c.action);
  res.success=success;res.duration_ms=millis()-started;res.write_calls=writesInCurrentOperation;copyReason(res,err);
  // Single reader: if response queue is full, drop oldest after noting failure upstream.
  if(xQueueSend(responses,&res,0)!=pdTRUE){BleResult discard;xQueueReceive(responses,&discard,0);xQueueSend(responses,&res,0);}
  lastHeartbeat=millis();
  linkTouched[static_cast<uint8_t>(c.device)==1?1:0]=millis();
 }
}
}
bool ble_begin(){
 if(requests)return true;
 requests=xQueueCreate(2,sizeof(cielo::ble::Command));responses=xQueueCreate(4,sizeof(BleResult));
 scanRequests=xQueueCreate(1,sizeof(uint8_t));scanResponses=xQueueCreate(2,sizeof(BleScanResult));
 probeRequests=xQueueCreate(1,sizeof(uint8_t));probeResponses=xQueueCreate(2,sizeof(BleProbeResult));
 releaseRequests=xQueueCreate(2,sizeof(uint8_t));
 customRequests=xQueueCreate(1,sizeof(uint8_t));
 customResults=xQueueCreate(1,sizeof(BleCustomResumeResult));
 if(!requests || !responses || !scanRequests || !scanResponses || !probeRequests || !probeResponses || !releaseRequests || !customRequests || !customResults)return false;
 return xTaskCreatePinnedToCore(task,"cielo_ble",16384,nullptr,1,nullptr,0)==pdPASS;
}
bool ble_custom_resume_request(){
 uint8_t token=1;
 return customRequests && xQueueSend(customRequests,&token,0)==pdTRUE;
}
bool ble_custom_resume_result(BleCustomResumeResult& result){
 return customResults && xQueueReceive(customResults,&result,0)==pdTRUE;
}
bool ble_release_request(uint8_t device){
 return device<2 && releaseRequests && xQueueSend(releaseRequests,&device,0)==pdTRUE;
}
BleLinkStatus ble_link_status(uint8_t device){
 BleLinkStatus r{};
 if(device>1)return r;
 r.connected=shared[device].connected;
 r.released_to_app=shared[device].released;
 r.release_count=shared[device].releases;
 r.last_disconnect_ms=shared[device].lastDisconnect;
 return r;
}
bool ble_probe_request(uint8_t device){
 return device<2 && probeRequests && xQueueSend(probeRequests,&device,0)==pdTRUE;
}
bool ble_probe_result(BleProbeResult& out){
 return probeResponses && xQueueReceive(probeResponses,&out,0)==pdTRUE;
}
bool ble_scan_request(){uint8_t token=1;return scanRequests && xQueueSend(scanRequests,&token,0)==pdTRUE;}
bool ble_scan_result(BleScanResult& out){return scanResponses && xQueueReceive(scanResponses,&out,0)==pdTRUE;}
bool ble_dispatch(const cielo::ble::Command& c){return requests && xQueueSend(requests,&c,0)==pdTRUE;}
bool ble_result(BleResult& r){return responses && xQueueReceive(responses,&r,0)==pdTRUE;}
uint32_t ble_last_heartbeat(){return lastHeartbeat;}
BleWorkerSnapshot ble_worker_snapshot(){
 BleWorkerSnapshot s{};
 const uint32_t now=millis();
 s.phase=workerPhase;s.device=workerDevice;
 s.phase_age_ms=now-phaseEnteredAt;
 s.heartbeat_age_ms=now-lastHeartbeat;
 s.in_operation=s.phase!=static_cast<uint8_t>(BleWorkerPhase::Idle);
 return s;
}
const char* ble_worker_phase_name(uint8_t p){
 switch(static_cast<BleWorkerPhase>(p)){
  case BleWorkerPhase::Starting:return "INICIANDO";
  case BleWorkerPhase::Idle:return "ESPERA";
  case BleWorkerPhase::Discovery:return "DESCUBRIMIENTO";
  case BleWorkerPhase::Connecting:return "CONEXION";
  case BleWorkerPhase::Service:return "SERVICIO_GATT";
  case BleWorkerPhase::Characteristic:return "CARACTERISTICA_GATT";
  case BleWorkerPhase::Writing:return "ESCRITURA_GATT";
  case BleWorkerPhase::Disconnecting:return "DESCONEXION";
  case BleWorkerPhase::Resuming:return "RETORNO_MY_CHIHIROS";
  case BleWorkerPhase::Probing:return "SONDA_GATT";
  case BleWorkerPhase::Sending:return "ORDEN_RGB";
  default:return "DESCONOCIDA";
 }
}
}
