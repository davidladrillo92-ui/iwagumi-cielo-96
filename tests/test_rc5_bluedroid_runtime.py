"""Execute patched connection/write functions and their late-event quarantine."""
from pathlib import Path
import subprocess,tempfile,importlib.util,shutil,hashlib,json
client=Path('vendor/bluedroid/BLEClient.cpp').read_text()
connect=client[client.index('bool BLEClient::connect(BLEAddress address,'):client.index('} // connect',client.index('bool BLEClient::connect(BLEAddress address,'))+len('} // connect')]
guard=client[client.index(' // REG callbacks are broadcast'):client.index('\n\tlog_d("gattClientEventHandler')]
write=Path('vendor/bluedroid/BLERemoteCharacteristic.cpp').read_text();a=write.index('bool BLERemoteCharacteristic::cieloWriteValue');write=write[a:write.index('} // writeValue',a)+1]
fixture=r'''
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <string>
using esp_err_t=int;using esp_ble_addr_type_t=int;
constexpr int ESP_OK=0,ESP_GATT_OK=0,ESP_GATT_ERROR=133,ESP_GATT_IF_NONE=255;
constexpr int ESP_GATTC_REG_EVT=1,ESP_GATTC_OPEN_EVT=2,ESP_GATTC_DISCONNECT_EVT=3,ESP_GATTC_UNREG_EVT=4;
constexpr int ESP_GATT_WRITE_TYPE_RSP=1,ESP_GATT_WRITE_TYPE_NO_RSP=2;
unsigned clockNow=0;unsigned millis(){return clockNow;}
#define pdMS_TO_TICKS(x) ((x)/10) // exercise conversion on non-1ms FreeRTOS ticks
struct BLEAddress {unsigned char b=0;unsigned char*getNative(){return &b;}};
struct Event {struct {int app_id=0,status=0;}reg;struct {int conn_id=7,status=0;}open;};
int removed=0,unregistered=0,closed=0,registerRc=0,openRc=0,writeRc=0,mode=0;
struct BLEDevice {static int m_appId;static void addPeerDevice(void*,bool,int){}static void removePeerDevice(int,bool){removed++;}};
int BLEDevice::m_appId=1;
int esp_ble_gattc_app_register(int){return registerRc;}
int esp_ble_gattc_app_unregister(int){unregistered++;return 0;}
int esp_ble_gattc_open(int,unsigned char,int,int){return openRc;}
int esp_ble_gattc_close(int,int){closed++;return 0;}
int esp_ble_gattc_write_char(int,int,int,size_t,unsigned char*,int,int){return writeRc;}
struct BLEClient;
BLEClient*active=nullptr;
struct Sem {bool available=true;unsigned val=0;
 bool take(unsigned,const char*){if(!available)return false;available=false;return true;}
 void give(unsigned v=0){val=v;available=true;}
 bool timedWait(const char*,unsigned ticks);unsigned value(){return val;}
};
struct BLEClient {
 std::atomic<bool> m_cieloPending{false},m_cieloExpired{false};
 std::atomic<uint8_t> m_cieloStage{0};std::atomic<int> m_cieloLastEspError{0};bool m_isConnected=false;
 int m_appId=0,m_gattc_if=ESP_GATT_IF_NONE,m_conn_id=7;BLEAddress m_peerAddress;
 Sem m_semaphoreRegEvt,m_semaphoreOpenEvt;
 BLEAddress&getPeerAddress(){return m_peerAddress;}
 bool isConnected(){return m_isConnected;}int getGattcIf(){return m_gattc_if;}int getConnId(){return m_conn_id;}
 bool connect(BLEAddress,esp_ble_addr_type_t);
 void event(int event,int gattc_if,Event*evtParam);
};
bool Sem::timedWait(const char*owner,unsigned ticks){
 std::string stage=owner;
 if((stage=="cielo-register"&&mode==1)||(stage=="cielo-open"&&mode==2)||(stage=="cielo-write"&&mode==6)){
  clockNow+=ticks*10;give();return false;
 }
 clockNow+=100;
 if(stage=="cielo-register")active->m_gattc_if=9;
 if(stage=="cielo-open"){active->m_isConnected=mode!=3;active->m_cieloPending=false;}
 give((mode==3&&stage=="cielo-open")|| (mode==5&&stage=="cielo-write")?ESP_GATT_ERROR:ESP_GATT_OK);
 return true;
}
struct Service {BLEClient*getClient(){return active;}} service;
struct BLERemoteCharacteristic {
 Service*m_pRemoteService=&service;Sem m_semaphoreWriteCharEvt;int m_auth=0;
 Service*getRemoteService(){return m_pRemoteService;}int getHandle(){return 3;}
 bool cieloWriteValue(unsigned char*,size_t,bool);
};
'''
main=r'''
int main(){
 for(unsigned start:{0U,0xfffffff0U}){
  clockNow=start;mode=0;BLEClient ok;active=&ok;assert(ok.connect({},0));assert(!ok.m_cieloPending);
  clockNow=start;mode=1;BLEClient reg;active=&reg;assert(!reg.connect({},0));assert(uint32_t(clockNow-start)==2000);assert(reg.m_cieloPending&&reg.m_cieloExpired);
  int id=reg.m_appId;assert(!reg.connect({},0)&&reg.m_appId==id); // no reuse while SDK pending
  Event e;e.reg.app_id=id+1;reg.event(ESP_GATTC_REG_EVT,9,&e);assert(reg.m_cieloPending); // foreign REG ignored
  e.reg.app_id=id;reg.event(ESP_GATTC_REG_EVT,9,&e);assert(!reg.m_cieloPending&&!reg.m_isConnected);
  mode=0;assert(reg.connect({},0)); // late failed registration does not poison the next attempt
  clockNow=start;mode=2;BLEClient open;active=&open;assert(!open.connect({},0));assert(uint32_t(clockNow-start)==8000);assert(open.m_cieloPending);
  e.open.status=ESP_GATT_OK;int n=closed;open.event(ESP_GATTC_OPEN_EVT,9,&e);assert(closed==n+1&&!open.m_isConnected&&open.m_cieloPending);
  open.event(ESP_GATTC_DISCONNECT_EVT,9,&e);assert(!open.m_cieloPending&&!open.m_isConnected);
  mode=0;assert(open.connect({},0));
 }
 mode=0;BLEClient c;active=&c;c.m_isConnected=true;
 unsigned char data[9]{};BLERemoteCharacteristic characteristic;
 assert(characteristic.cieloWriteValue(data,9,true));
 mode=5;assert(!characteristic.cieloWriteValue(data,9,true)); // GATT error status must fail
 mode=0;writeRc=1;assert(!characteristic.cieloWriteValue(data,9,false));assert(characteristic.m_semaphoreWriteCharEvt.available);
 writeRc=0;assert(characteristic.cieloWriteValue(data,9,false)); // immediate SDK error does not strand semaphore
 mode=6;auto start=clockNow;assert(!characteristic.cieloWriteValue(data,9,true));assert(clockNow-start==8000);
 c.m_isConnected=false;assert(!characteristic.cieloWriteValue(data,9,true));
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);source=p/'test.cpp';source.write_text(fixture+connect+'\nvoid BLEClient::event(int event,int gattc_if,Event*evtParam){'+guard+'}\n'+write+main)
 binary=str(p/'test');subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',str(source),'-o',binary],check=True);subprocess.run([binary],check=True)
 # Build preparation verifies upstream and never modifies the installed SDK.
 spec=importlib.util.spec_from_file_location('prep','tools/prepare_bluedroid.py');prep=importlib.util.module_from_spec(spec);spec.loader.exec_module(prep)
 framework=p/'framework';sdk=framework/'libraries/BLE/src';sdk.mkdir(parents=True)
 for file in Path('vendor/bluedroid/upstream').iterdir():shutil.copy2(file,sdk/file.name)
 project=p/'project';shutil.copytree('vendor',project/'vendor')
 before={f.name:f.read_bytes() for f in sdk.iterdir()};dest=prep.prepare(framework,project)
 assert all((sdk/n).read_bytes()==b for n,b in before.items())
 assert (dest/'src/BLEClient.cpp').read_bytes()==Path('vendor/bluedroid/BLEClient.cpp').read_bytes()
 assert json.loads((dest/'library.json').read_text())['name']=='CieloBluedroid'
 (sdk/'BLEClient.cpp').write_text('wrong SDK')
 try:prep.prepare(framework,project)
 except RuntimeError:pass
 else:raise AssertionError('unexpected upstream accepted')
print('PASS actual Bluedroid patch: 2s/8s deadlines, rollover, quarantine, late callbacks, retry, SDK/GATT write failures and isolated pinned build')
