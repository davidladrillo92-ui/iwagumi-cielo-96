#include "cielo_ble_worker.h"
#include "cielo_codec.h"
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
 BLEClient* client=nullptr;
 BLERemoteCharacteristic* writer=nullptr;
 bool manualEntered=false;
 bool pvOn=false;
};
Target targets[2]={{codec::PV_NAME,codec::PV_SVC,codec::PV_TX},
                   {codec::CH_NAME,codec::CH_SVC,codec::CH_TX}};
QueueHandle_t requests=nullptr,responses=nullptr;
volatile uint32_t lastHeartbeat=0;
uint16_t chMessageId=1;
// Exactly one FreeRTOS task owns all BLE calls. Web thread NEVER interacts with the stack.
void copyReason(BleResult& r,const char* msg){strncpy(r.reason,msg,sizeof(r.reason)-1);r.reason[sizeof(r.reason)-1]='\0';}
bool locate(Target& t) {
 if(t.addressKnown) return true;
 BLEScan* scan=BLEDevice::getScan();
 scan->setActiveScan(true);
 scan->setInterval(150);scan->setWindow(75);
 BLEScanResults result=scan->start(3,false);
 bool found=false;
 for(int i=0;i<result.getCount();++i){
  BLEAdvertisedDevice d=result.getDevice(i);
  if(d.haveName() && String(d.getName().c_str())==String(t.name)){
   t.address=String(d.getAddress().toString().c_str());t.addressKnown=true;found=true;break;
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
 if(!t.client->connect(address)){
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
void task(void*){
 BLEDevice::init("IWAGUMI-CIELO-96");
 lastHeartbeat=millis();
 for(;;){
  cielo::ble::Command c{};
  // Block without consuming CPU; heartbeat updates every second when idle.
  if(xQueueReceive(requests,&c,pdMS_TO_TICKS(1000))!=pdTRUE){lastHeartbeat=millis();continue;}
  uint32_t started=millis();const char*err="OK";
  bool success=(c.device==cielo::ble::Device::PezVerde)?sendPez(c,err):sendCh(c,err);
  BleResult res{};res.sequence=c.sequence;res.device=static_cast<uint8_t>(c.device);
  res.success=success;res.duration_ms=millis()-started;copyReason(res,err);
  // Single reader: if response queue is full, drop oldest after noting failure upstream.
  if(xQueueSend(responses,&res,0)!=pdTRUE){BleResult discard;xQueueReceive(responses,&discard,0);xQueueSend(responses,&res,0);}
  lastHeartbeat=millis();
 }
}
}
bool ble_begin(){
 if(requests)return true;
 requests=xQueueCreate(2,sizeof(cielo::ble::Command));responses=xQueueCreate(4,sizeof(BleResult));
 if(!requests || !responses)return false;
 return xTaskCreatePinnedToCore(task,"cielo_ble",16384,nullptr,1,nullptr,0)==pdPASS;
}
bool ble_dispatch(const cielo::ble::Command& c){return requests && xQueueSend(requests,&c,0)==pdTRUE;}
bool ble_result(BleResult& r){return responses && xQueueReceive(responses,&r,0)==pdTRUE;}
uint32_t ble_last_heartbeat(){return lastHeartbeat;}
}
