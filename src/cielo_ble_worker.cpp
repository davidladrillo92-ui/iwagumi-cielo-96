#include "cielo_ble_worker.h"
#include "cielo_codec.h"
#include "cielo_ble_match.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEClient.h>
#include <BLERemoteCharacteristic.h>
#include <string.h>
#include <atomic>

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
struct SharedLink { std::atomic<bool> connected{false},released{false},idleReleased{false},expectedRelease{false};std::atomic<uint32_t> releases{0},lastDisconnect{0},pendingSince{0};};
SharedLink shared[2]{};
std::atomic<bool> pvOffRequired{false};
// Healthy BLE connections remain owned by CIELO; the web thread only sets flags.
std::atomic<bool> keepLinks[2]{};
std::atomic<uint32_t> unexpectedLoss[2]{},lastUnexpectedLoss[2]{};
bool offWrittenInCurrentOperation=false;
struct CallbackCounters {std::atomic<uint32_t> connected{0},disconnected{0},lastConnect{0},lastDisconnect{0};};
CallbackCounters callbackCounters[2]{};
struct LinkCallbacks: BLEClientCallbacks {
 uint8_t device;explicit LinkCallbacks(uint8_t d):device(d){}
 void onConnect(BLEClient*) override {callbackCounters[device].connected.fetch_add(1);callbackCounters[device].lastConnect.store(millis());shared[device].connected=true;shared[device].idleReleased=false;shared[device].expectedRelease=false;}
 void onDisconnect(BLEClient*) override {
  callbackCounters[device].disconnected.fetch_add(1);callbackCounters[device].lastDisconnect.store(millis());
  shared[device].connected=false;shared[device].lastDisconnect=millis();
  if(!shared[device].expectedRelease.exchange(false)){
    shared[device].idleReleased=false;
    unexpectedLoss[device].fetch_add(1);lastUnexpectedLoss[device].store(millis());
   }
 }
};
LinkCallbacks linkCallbacks[2]={LinkCallbacks(0),LinkCallbacks(1)};

// Escritos solo desde la tarea BLE; enteros de 32 bits leídos pasivamente desde HTTP.
struct PhaseCounters {
 volatile uint32_t connect_count=0,connect_failed=0,connect_last=0,connect_max=0,connect_timeouts=0;
 volatile uint32_t write_count=0,write_failed=0,write_last=0,write_max=0;
 volatile uint32_t disconnect_count=0,disconnect_last=0,disconnect_max=0;
};
PhaseCounters phaseCounters[2]{};
inline void metricTime(volatile uint32_t&last,volatile uint32_t&maxVal,uint32_t duration){
 last=duration;if(duration>maxVal)maxVal=duration;
}

uint32_t linkTouched[2]{};
// Reuse links across 30/60s curve updates; explicit app/OTA release is immediate.
static constexpr uint32_t BLE_IDLE_RELEASE_MS=90000UL;
TaskHandle_t workerHandle=nullptr;
volatile uint32_t lastHeartbeat=0;
volatile uint8_t workerPhase=static_cast<uint8_t>(BleWorkerPhase::Starting);
volatile uint8_t workerDevice=255;
volatile uint32_t phaseEnteredAt=0;
portMUX_TYPE snapshotMux=portMUX_INITIALIZER_UNLOCKED;
BlePhaseEvent phaseHistory[16]{};
uint8_t phaseHistoryHead=0,phaseHistoryCount=0;
uint32_t phaseOrdinal=0;
void touchHeartbeat(){portENTER_CRITICAL(&snapshotMux);lastHeartbeat=millis();portEXIT_CRITICAL(&snapshotMux);}
void setWorkerPhase(BleWorkerPhase p,uint8_t dev=255){
 portENTER_CRITICAL(&snapshotMux);
 const uint32_t now=millis();
 workerDevice=dev;phaseEnteredAt=now;workerPhase=static_cast<uint8_t>(p);
 if(!phaseHistoryCount || phaseHistory[(phaseHistoryHead+15)%16].phase!=static_cast<uint8_t>(p) ||
    phaseHistory[(phaseHistoryHead+15)%16].device!=dev){
  phaseHistory[phaseHistoryHead]={now,++phaseOrdinal,static_cast<uint8_t>(p),dev};
  phaseHistoryHead=(phaseHistoryHead+1)%16;
  if(phaseHistoryCount<16)phaseHistoryCount++;
 }
 portEXIT_CRITICAL(&snapshotMux);
}
void releaseTarget(uint8_t device,bool userRequested){
 setWorkerPhase(BleWorkerPhase::Disconnecting,device);
 Target& t=targets[device];
 const bool wasConnected=t.client && t.client->isConnected();
 shared[device].expectedRelease=wasConnected;shared[device].idleReleased=!userRequested;
 if(wasConnected){
  const uint32_t started=millis();
  t.client->disconnect();
  PhaseCounters& metrics=phaseCounters[device];metrics.disconnect_count++;
  metricTime(metrics.disconnect_last,metrics.disconnect_max,millis()-started);
 }
 t.writer=nullptr;t.manualEntered=false;t.pvOn=false;
 // Mantener direccion/tipo aprendidos para evitar scan de 5s en cada orden;
 // ante un fallo real de conexion se invalida en connect().
 shared[device].connected=false;
 shared[device].released=userRequested;
 shared[device].releases++;
 shared[device].lastDisconnect=millis();
 linkTouched[device]=0;
}
void reconcileLinks(){
 for(uint8_t device=0;device<2;device++){
  Target& t=targets[device];
  const bool connected=t.client&&t.client->isConnected();
  if(!connected){
   if(shared[device].connected){
     shared[device].lastDisconnect=millis();shared[device].idleReleased=false;
     if(!shared[device].expectedRelease.exchange(false)){
      unexpectedLoss[device].fetch_add(1);lastUnexpectedLoss[device].store(millis());
     }
    }
   shared[device].connected=false;t.writer=nullptr;t.manualEntered=false;t.pvOn=false;
  }else shared[device].connected=t.writer!=nullptr;
  if(t.client&&t.client->cieloConnectPending()){
   if(!shared[device].pendingSince)shared[device].pendingSince=millis();
  }else shared[device].pendingSince=0;
 }
}
void releaseIdleLinks(){
 uint32_t now=millis();
 for(uint8_t k=0;k<2;k++)
  if(targets[k].client && targets[k].client->isConnected() && !keepLinks[k].load())
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
 t.writer=nullptr;t.manualEntered=false;t.pvOn=false;
 if(t.client&&t.client->cieloConnectPending()){err="BLE_CONNECT_CLEANUP_PENDING";return false;}
 if(!locate(t)){err="BLE_DEVICE_NOT_FOUND";return false;}
 if(!t.client){t.client=BLEDevice::createClient();if(t.client)t.client->setClientCallbacks(&linkCallbacks[&t==&targets[0]?0:1]);}
 if(!t.client){err="BLE_CLIENT_CREATE";return false;}
 // connect(BLEAddress) on Arduino-ESP32 2.x; this may block, isolated from HTTP.
 BLEAddress address(std::string(t.address.c_str()));
 setWorkerPhase(BleWorkerPhase::Connecting,&t==&targets[0]?0:1);
 const uint32_t started=millis();
 bool connected=t.client->connect(address,t.addressType);
 PhaseCounters& metrics=phaseCounters[&t==&targets[0]?0:1];
 metrics.connect_count++;if(!connected){metrics.connect_failed++;if(t.client->cieloConnectExpired())metrics.connect_timeouts++;}
 metricTime(metrics.connect_last,metrics.connect_max,millis()-started);
 if(!connected){
  err=t.client->cieloConnectExpired()?"BLE_CONNECT_TIMEOUT":"BLE_CONNECT_FAILED";
  t.addressKnown=false;shared[&t==&targets[0]?0:1].connected=false;return false;
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
 const uint32_t writeStarted=millis();
 const bool accepted=t.writer->cieloWriteValue(const_cast<uint8_t*>(data),n,t.writer->canWrite());
 PhaseCounters& metrics=phaseCounters[&t==&targets[0]?0:1];metrics.write_count++;
 metricTime(metrics.write_last,metrics.write_max,millis()-writeStarted);
 if(writesInCurrentOperation<255)writesInCurrentOperation++; // GATT write call, not optical acknowledgement
 if(!accepted||!t.client->isConnected()){metrics.write_failed++;err="GATT_WRITE_FAILED";t.writer=nullptr;t.client->disconnect();return false;}
 vTaskDelay(pdMS_TO_TICKS(30));
 return true;
}
bool sendPez(const cielo::ble::Command& c,const char*&err){
 Target&t=targets[0];if(!connect(t,err))return false;
 uint8_t frame[9];
 auto sendOff=[&](){codec::pv_power(false,frame);if(!put(t,frame,9,err))return false;
  t.pvOn=false;offWrittenInCurrentOperation=true;return true;};
 if(c.brightness==0||pvOffRequired.load())return sendOff();
 // The controller can be switched off externally while the BLE link survives.
 // Reassert the verified ON frame instead of trusting a cached power state.
 codec::pv_power(true,frame);if(!put(t,frame,9,err))return false;t.pvOn=true;
 if(pvOffRequired.load())return sendOff();
 codec::pv_color(c.r,c.g,c.b,frame);if(!put(t,frame,9,err))return false;
 if(pvOffRequired.load())return sendOff();
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
  if(!t.client){t.client=BLEDevice::createClient();if(t.client)t.client->setClientCallbacks(&linkCallbacks[&t==&targets[0]?0:1]);}
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
 touchHeartbeat();
 for(;;){
  setWorkerPhase(BleWorkerPhase::Idle);
  touchHeartbeat();
  uint8_t probeToken=0;
  uint8_t releaseToken=0;
  while(xQueueReceive(releaseRequests,&releaseToken,0)==pdTRUE){
   if(releaseToken<2)releaseTarget(releaseToken,true);
  }
  reconcileLinks();
  releaseIdleLinks();
  uint8_t customToken=0;
  if(xQueueReceive(customRequests,&customToken,0)==pdTRUE){
   resumeCustomCurve();touchHeartbeat();
  }
  if(xQueueReceive(probeRequests,&probeToken,0)==pdTRUE){
   probeGATT(probeToken);touchHeartbeat();
  }
  uint8_t scanToken=0;
  if(xQueueReceive(scanRequests,&scanToken,0)==pdTRUE){
   scanDiscover();touchHeartbeat();
  }
  cielo::ble::Command c{};
  // Block without consuming CPU; heartbeat updates every second when idle.
  if(xQueueReceive(requests,&c,pdMS_TO_TICKS(1000))!=pdTRUE){touchHeartbeat();continue;}
  setWorkerPhase(BleWorkerPhase::Sending,static_cast<uint8_t>(c.device));
  uint32_t started=millis();writesInCurrentOperation=0;offWrittenInCurrentOperation=false;const char*err="OK";
  bool success=(c.device==cielo::ble::Device::PezVerde)?sendPez(c,err):sendCh(c,err);
  BleResult res{};res.sequence=c.sequence;res.device=static_cast<uint8_t>(c.device);res.action=static_cast<uint8_t>(c.action);
  res.off_sent=offWrittenInCurrentOperation;res.success=success;res.duration_ms=millis()-started;res.write_calls=writesInCurrentOperation;copyReason(res,err);
  // Single reader: if response queue is full, drop oldest after noting failure upstream.
  if(xQueueSend(responses,&res,0)!=pdTRUE){BleResult discard;xQueueReceive(responses,&discard,0);xQueueSend(responses,&res,0);}
  touchHeartbeat();
  const uint8_t device=static_cast<uint8_t>(c.device)==1?1:0;
  linkTouched[device]=millis();
  // Bounded transactions: always leave the controller free after the full batch.
  if((!keepLinks[device].load() || !success) &&
     (!targets[device].client || !targets[device].client->cieloConnectPending()))
    releaseTarget(device,false);
 }
}
}
// B9: inicio transaccional; una asignación parcial nunca simula un worker vivo.
bool ble_begin(){
 if(workerHandle)return true;
 QueueHandle_t* handles[]={&requests,&responses,&scanRequests,&scanResponses,
  &probeRequests,&probeResponses,&releaseRequests,&customRequests,&customResults};
 const UBaseType_t lengths[]={2,4,1,2,1,2,2,1,1};
 const UBaseType_t sizes[]={sizeof(cielo::ble::Command),sizeof(BleResult),sizeof(uint8_t),
  sizeof(BleScanResult),sizeof(uint8_t),sizeof(BleProbeResult),sizeof(uint8_t),sizeof(uint8_t),sizeof(BleCustomResumeResult)};
 for(size_t i=0;i<9;i++){
  *handles[i]=xQueueCreate(lengths[i],sizes[i]);
  if(!*handles[i]){
   for(size_t j=0;j<i;j++){vQueueDelete(*handles[j]);*handles[j]=nullptr;}
   return false;
  }
 }
 if(xTaskCreatePinnedToCore(task,"cielo_ble",16384,nullptr,1,&workerHandle,0)==pdPASS)return true;
 workerHandle=nullptr;
 for(auto handle:handles){vQueueDelete(*handle);*handle=nullptr;}
 return false;
}
uint32_t ble_stack_free_bytes(){
 // ESP-IDF FreeRTOS expresa uxTaskGetStackHighWaterMark en bytes.
 return workerHandle?uxTaskGetStackHighWaterMark(workerHandle):0;
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
 r.idle_released=shared[device].idleReleased;
 const uint32_t pending=shared[device].pendingSince;
 r.connection_pending=pending!=0;r.pending_age_ms=pending?uint32_t(millis()-pending):0;
 r.released_to_app=shared[device].released;
 r.release_count=shared[device].releases;
 r.last_disconnect_ms=shared[device].lastDisconnect;
 r.unexpected_disconnects=unexpectedLoss[device].load();
 r.last_unexpected_ms=lastUnexpectedLoss[device].load();
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
void ble_set_pv_off_required(bool value){pvOffRequired.store(value);}
void ble_keep_links(bool pv,bool ch){
 keepLinks[0].store(pv);keepLinks[1].store(ch);
} // Worker exclusively performs actual disconnects; OTA/app release sets false.
bool ble_dispatch(const cielo::ble::Command& c){return requests && xQueueSend(requests,&c,0)==pdTRUE;}
bool ble_result(BleResult& r){return responses && xQueueReceive(responses,&r,0)==pdTRUE;}
uint32_t ble_last_heartbeat(){return lastHeartbeat;}
BleTransportStats ble_transport_stats(uint8_t device){
 BleTransportStats r{};if(device>1)return r;
 const PhaseCounters& m=phaseCounters[device];
 r.connect_count=m.connect_count;r.connect_failed=m.connect_failed;r.connect_timeouts=m.connect_timeouts;
 r.connect_last_ms=m.connect_last;r.connect_max_ms=m.connect_max;
 r.write_count=m.write_count;r.write_failed=m.write_failed;
 r.write_last_ms=m.write_last;r.write_max_ms=m.write_max;
 r.disconnect_count=m.disconnect_count;r.disconnect_last_ms=m.disconnect_last;
 r.disconnect_max_ms=m.disconnect_max;
 return r;
}
uint32_t ble_request_queue_depth(){return requests?uxQueueMessagesWaiting(requests):0;}
BleWorkerSnapshot ble_worker_snapshot(){
 BleWorkerSnapshot s{};
 portENTER_CRITICAL(&snapshotMux);
 const uint32_t now=millis();
 s.phase=workerPhase;s.device=workerDevice;
 s.phase_age_ms=now-phaseEnteredAt;
 s.heartbeat_age_ms=now-lastHeartbeat;
 portEXIT_CRITICAL(&snapshotMux);
 s.in_operation=s.phase!=static_cast<uint8_t>(BleWorkerPhase::Idle);
 return s;
}
BleCallbackStats ble_callback_stats(uint8_t device){
 BleCallbackStats out{};
 if(device>1)return out;
 out.on_connect=callbackCounters[device].connected.load();
 out.on_disconnect=callbackCounters[device].disconnected.load();
 out.last_connect_ms=callbackCounters[device].lastConnect.load();
 out.last_disconnect_ms=callbackCounters[device].lastDisconnect.load();
 return out;
}
BlePhaseHistory ble_phase_history(){
 BlePhaseHistory out{};
 portENTER_CRITICAL(&snapshotMux);
 out.count=phaseHistoryCount;
 for(uint8_t i=0;i<out.count;i++)out.events[i]=phaseHistory[(phaseHistoryHead+16-out.count+i)%16];
 portEXIT_CRITICAL(&snapshotMux);
 return out;
}
BleBlockageSnapshot ble_blockage_snapshot(){
 BleBlockageSnapshot result{};
 result.worker=ble_worker_snapshot();
 const uint32_t now=millis();
 const uint32_t pv=shared[0].pendingSince.load();
 const uint32_t ch=shared[1].pendingSince.load();
 result.pending_pv_age_ms=pv?uint32_t(now-pv):0;
 result.pending_ch_age_ms=ch?uint32_t(now-ch):0;
 return result;
}
// A pending expired SDK request is separate from a hung worker. The recovery
// supervisor alone uses its age; normal HTTP telemetry keeps the real heartbeat.
BleWorkerSnapshot ble_recovery_snapshot(){
 auto s=ble_worker_snapshot();
 for(uint8_t device=0;device<2;device++){
  const uint32_t started=shared[device].pendingSince;
  const uint32_t age=started?uint32_t(millis()-started):0;
  if(age>s.phase_age_ms){s.phase=static_cast<uint8_t>(BleWorkerPhase::Connecting);s.device=device;s.phase_age_ms=age;s.heartbeat_age_ms=age;}
 }
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
