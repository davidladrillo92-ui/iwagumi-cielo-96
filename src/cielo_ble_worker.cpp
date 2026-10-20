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
QueueHandle_t requests=nullptr,responses=nullptr,scanRequests=nullptr,scanResponses=nullptr;
volatile uint32_t lastHeartbeat=0;
uint16_t chMessageId=1;
uint8_t writesInCurrentOperation=0;
// Exactly one FreeRTOS task owns all BLE calls. Web thread NEVER interacts with the stack.
void copyReason(BleResult& r,const char* msg){strncpy(r.reason,msg,sizeof(r.reason)-1);r.reason[sizeof(r.reason)-1]='\0';}
bool locate(Target& t) {
 if(t.addressKnown) return true;
 BLEScan* scan=BLEDevice::getScan();
 scan->setActiveScan(true);
 scan->setInterval(150);scan->setWindow(75);
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
 if(t.client && t.client->isConnected() && t.writer)return true;
 t.writer=nullptr;t.manualEntered=false;
 if(!locate(t)){err="BLE_DEVICE_NOT_FOUND";return false;}
 if(!t.client)t.client=BLEDevice::createClient();
 if(!t.client){err="BLE_CLIENT_CREATE";return false;}
 // connect(BLEAddress) on Arduino-ESP32 2.x; this may block, isolated from HTTP.
 BLEAddress address(std::string(t.address.c_str()));
 if(!t.client->connect(address,t.addressType)){
  err="BLE_CONNECT_FAILED";t.addressKnown=false;return false;
 }
 BLERemoteService* service=t.client->getService(BLEUUID(t.service));
 if(!service){err="GATT_SERVICE_MISSING";t.client->disconnect();return false;}
 t.writer=service->getCharacteristic(BLEUUID(t.writeUuid));
 if(!t.writer || !(t.writer->canWrite()||t.writer->canWriteNoResponse())){
  err="GATT_CHARACTERISTIC_MISSING";t.client->disconnect();t.writer=nullptr;return false;
 }
 return true;
}
bool put(Target& t,const uint8_t* data,size_t n,const char*&err){
 if(!t.client || !t.client->isConnected() || !t.writer){err="GATT_NOT_CONNECTED";return false;}
 // Arduino BLE writeValue returns void: transport accepted != optical ACK!
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
  if(!codec::ch_auto(chMessageId,frame,sizeof(frame),size)){err="CH_AUTO_ENCODE";return false;}
  if(!put(t,frame,size,err))return false;
  t.manualEntered=false;return true;
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
void task(void*){
 BLEDevice::init("IWAGUMI-CIELO-96");
 lastHeartbeat=millis();
 for(;;){
  uint8_t scanToken=0;
  if(xQueueReceive(scanRequests,&scanToken,0)==pdTRUE){
   scanDiscover();lastHeartbeat=millis();
  }
  cielo::ble::Command c{};
  // Block without consuming CPU; heartbeat updates every second when idle.
  if(xQueueReceive(requests,&c,pdMS_TO_TICKS(1000))!=pdTRUE){lastHeartbeat=millis();continue;}
  uint32_t started=millis();writesInCurrentOperation=0;const char*err="OK";
  bool success=(c.device==cielo::ble::Device::PezVerde)?sendPez(c,err):sendCh(c,err);
  BleResult res{};res.sequence=c.sequence;res.device=static_cast<uint8_t>(c.device);
  res.success=success;res.duration_ms=millis()-started;res.write_calls=writesInCurrentOperation;copyReason(res,err);
  // Single reader: if response queue is full, drop oldest after noting failure upstream.
  if(xQueueSend(responses,&res,0)!=pdTRUE){BleResult discard;xQueueReceive(responses,&discard,0);xQueueSend(responses,&res,0);}
  lastHeartbeat=millis();
 }
}
}
bool ble_begin(){
 if(requests)return true;
 requests=xQueueCreate(2,sizeof(cielo::ble::Command));responses=xQueueCreate(4,sizeof(BleResult));
 scanRequests=xQueueCreate(1,sizeof(uint8_t));scanResponses=xQueueCreate(2,sizeof(BleScanResult));
 if(!requests || !responses || !scanRequests || !scanResponses)return false;
 return xTaskCreatePinnedToCore(task,"cielo_ble",16384,nullptr,1,nullptr,0)==pdPASS;
}
bool ble_scan_request(){uint8_t token=1;return scanRequests && xQueueSend(scanRequests,&token,0)==pdTRUE;}
bool ble_scan_result(BleScanResult& out){return scanResponses && xQueueReceive(scanResponses,&out,0)==pdTRUE;}
bool ble_dispatch(const cielo::ble::Command& c){return requests && xQueueSend(requests,&c,0)==pdTRUE;}
bool ble_result(BleResult& r){return responses && xQueueReceive(responses,&r,0)==pdTRUE;}
uint32_t ble_last_heartbeat(){return lastHeartbeat;}
}
