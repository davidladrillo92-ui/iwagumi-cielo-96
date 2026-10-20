// IWAGUMI CIELO 9.6 R4 development firmware
// ESP32-S3 / ATOMS3 Lite C124. Source implementation; hardware NOT YET VERIFIED.
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>
#include <Update.h>
#include <time.h>
#include <sys/time.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>
#include "cielo_engine.h"
#include "profiles_generated.h"
#include "cielo_profile_store.h"
#include "cielo_ble_queue.h"
#include "cielo_ble_worker.h"
#include "cielo_ui.h"

#if __has_include("local_config.h")
#include "local_config.h"
#endif
#ifndef CIELO_WIFI_SSID
#define CIELO_WIFI_SSID ""
#endif
#ifndef CIELO_WIFI_PASSWORD
#define CIELO_WIFI_PASSWORD ""
#endif
#ifndef CIELO_ENABLE_OTA
#define CIELO_ENABLE_OTA 0 // Explicitly enable only after bench validation
#endif

static constexpr const char* VERSION="9.6.0-R4.3.3-RESCATE";
static WebServer srv(80);
static Preferences prefs;
static cielo96::ProfileImage profile;
static cielo::ble::Scheduler queue(3500,250,2000);
static bool bleReady=false, profileRecovery=false, wifiApOn=false;
static bool blePilotRequested=false, blePilotAttempted=false, rescueOpen=false;
static bool apProtected=false;
static uint32_t rescueUntil=0, lastIndicatorAt=0;
static constexpr uint8_t CIELO_LED_GPIO=35; // AtomS3 Lite C124: onboard WS2812C
static Adafruit_NeoPixel statusPixel(1,CIELO_LED_GPIO,NEO_GRB+NEO_KHZ800);
static void statusLedTick(){
 const uint32_t now=millis();
 if(now-lastIndicatorAt<600)return;
 lastIndicatorAt=now;
 uint32_t color=0;
 if(rescueOpen)color=statusPixel.Color(15,0,20); // Violet: read-only rescue
 else if(!wifiApOn)color=statusPixel.Color(17,5,0); // Amber: AP startup
 else if(WiFi.status()!=WL_CONNECTED)color=statusPixel.Color(0,0,17); // Blue: private AP
 else if(!bleReady)color=statusPixel.Color(0,17,17); // Cyan: Wi-Fi ready / BLE off
 else color=statusPixel.Color(0,17,0); // Green: BLE worker initialized
 statusPixel.setPixelColor(0,color);statusPixel.show();
}
static bool pezManual=false,chiManual=false,chiCieloNight=true,chiAutoPending=false;
static uint32_t pezManualUntil=0,chiManualUntil=0;
static uint32_t lastScheduleAt=0,lastWiFiAttempt=0,lastStatusAt=0,otaBytes=0,otaRestarts=0;
static uint32_t lastPvDesiredAt=0,lastChDesiredAt=0;
static uint32_t httpCount=0,bleResultCount=0,bleResultErrors=0,bleSoftTimeouts=0;
static uint32_t bleSlowCount=0,bleDurationMax=0,wifiFailures=0;
struct DevTelemetry {uint32_t completed=0,gatt_ok=0,write_calls=0,errors=0,slow=0,last_ms=0,max_ms=0;};
static DevTelemetry devices[2]; // PV and Chihiros. GATT acceptance is not an optical ACK.
static bool lastPvDesired=false,lastChDesired=false,chiAutoAttempted=false;
static cielo::PvValue prevPv{};
static cielo::ChiValue prevCh{};
static bool otaBusy=false,otaAllowed=false,otaSuccess=false,otaError=false;
static String lastBleError="ninguno",lastBackupError="ninguno",lastOtaError="Sin actualizaciones";
static String storedSSID,storedPass,apPassword;
static uint32_t bootMs=0;
static constexpr int ATOMS3_BUTTON_GPIO=41;
static uint32_t buttonDownAt=0,otaPhysicalUntil=0;
static bool physicalArmed=false;
static uint32_t previousBootCount=0;
static String priorEvent="ninguno",priorEventDetail="ninguno";
static bool priorBootClean=false;
static bool currentBootMarkedHealthy=false;
static const char* resetReasonName(esp_reset_reason_t r){
 switch(r){
  case ESP_RST_POWERON:return "POWER_ON";
  case ESP_RST_EXT:return "EXTERNAL_RESET";
  case ESP_RST_SW:return "SOFTWARE_RESTART";
  case ESP_RST_PANIC:return "PANIC_EXCEPTION";
  case ESP_RST_INT_WDT:return "INTERRUPT_WATCHDOG";
  case ESP_RST_TASK_WDT:return "TASK_WATCHDOG";
  case ESP_RST_WDT:return "OTHER_WATCHDOG";
  case ESP_RST_BROWNOUT:return "BROWNOUT";
  default:return "OTHER_OR_UNKNOWN";
 }
}
static bool abnormalReset(esp_reset_reason_t r){
 return r==ESP_RST_PANIC||r==ESP_RST_INT_WDT||r==ESP_RST_TASK_WDT||
        r==ESP_RST_WDT||r==ESP_RST_BROWNOUT;
}

static uint32_t savedFailures=0;
static uint32_t lastPersistentFaultAt=0;
static uint32_t latencyHistory[2][32]{},latencyCount[2]{};
static uint32_t latencyP95(uint8_t device){
 const uint32_t n=latencyCount[device]<32?latencyCount[device]:32;
 if(!n)return 0;
 uint32_t sorted[32]{};
 for(uint32_t i=0;i<n;i++)sorted[i]=latencyHistory[device][i];
 for(uint32_t i=1;i<n;i++){
  uint32_t key=sorted[i];int j=int(i)-1;
  while(j>=0&&sorted[j]>key){sorted[j+1]=sorted[j];--j;}sorted[j+1]=key;
 }
 return sorted[(95*n+99)/100-1];
}
static uint32_t wifiDisconnects=0;
static wl_status_t lastWifiStatus=WL_IDLE_STATUS;
static bool connectionStarted=false;
static bool apRecoveryAlways=true; // Rescue AP remains available while STA is connected.
static String otaRunningPartition,otaNextPartition;
static uint32_t lastEventId=0;
static constexpr int EVENT_MAX=36;
struct Event{uint32_t elapsed;char label[22];char detail[82];};
static Event events[EVENT_MAX];static int eventCount=0,eventHead=0;

static void persistCritical(const char* name,const String& detail){
 // Persist only faults, never minute samples. Throttle NVS wear to one write per 60 s.
 if(millis()-lastPersistentFaultAt<60000UL)return;
 lastPersistentFaultAt=millis();
 prefs.putString("fault_type",name);
 prefs.putString("fault_text",detail.substring(0,70));
 prefs.putUInt("fault_count",prefs.getUInt("fault_count",0)+1);
}
static void record(const char*name,const String& detail){
 Event&e=events[eventHead];e.elapsed=millis()-bootMs;
 snprintf(e.label,sizeof(e.label),"%s",name);
 snprintf(e.detail,sizeof(e.detail),"%s",detail.c_str());
 eventHead=(eventHead+1)%EVENT_MAX;if(eventCount<EVENT_MAX)eventCount++;lastEventId++;
 Serial.printf("CIELO EVENT %s %s\n",name,detail.c_str());
 if(strncmp(name,"BLE_ERROR",9)==0 || strncmp(name,"CURVA_ERROR",11)==0 ||
    strncmp(name,"BLE_SOFT_TIMEOUT",16)==0 || strncmp(name,"OTA_ERROR",9)==0)
   persistCritical(name,detail);
}
static String jsonQuote(const String& s){
 String r="\"";for(size_t i=0;i<s.length();i++){
 char c=s[i];if(c=='"'||c=='\\'){r+='\\';r+=c;}else if(c=='\n')r+="\\n";
 else if((unsigned char)c<32)r+=' ';else r+=c;
 }return r+'"';
}
static void jsonSend(int code,const String& s){++httpCount;srv.sendHeader("Cache-Control","no-store, no-cache, must-revalidate");srv.send(code,"application/json; charset=utf-8",s);}
static void fail(int code,const char*msg){jsonSend(code,String("{\"ok\":false,\"error\":")+jsonQuote(msg)+"}");}
static void ok(){jsonSend(200,"{\"ok\":true}");}
static bool timeOK(){return time(nullptr)>1700000000L;}
static int minuteOfDay(){time_t t=time(nullptr);struct tm x{};localtime_r(&t,&x);return x.tm_hour*60+x.tm_min;}
static String dateTime(){
 if(!timeOK())return "Hora pendiente NTP";
 time_t t=time(nullptr);struct tm x{};localtime_r(&t,&x);
 char b[32];snprintf(b,sizeof(b),"%02d/%02d/%04d %02d:%02d:%02d",x.tm_mday,x.tm_mon+1,x.tm_year+1900,x.tm_hour,x.tm_min,x.tm_sec);
 return b;
}
static String pointsPv(){String s;s.reserve(profile.pvCount*25);
 for(unsigned i=0;i<profile.pvCount;i++){const auto&p=profile.pv[i];if(i)s+=';';
 s+=String(p.minute)+','+String(p.r)+','+String(p.g)+','+String(p.b)+','+String(p.brightness);
 }return s;
}
static String pointsCh(){String s;s.reserve(profile.chCount*20);
 for(unsigned i=0;i<profile.chCount;i++){const auto&p=profile.ch[i];if(i)s+=';';
 s+=String(p.minute)+','+String(p.r)+','+String(p.g)+','+String(p.b);
 }return s;
}
static cielo::PvValue desiredPv(){return timeOK()?cielo::at(profile.pv,profile.pvCount,minuteOfDay()):cielo::PvValue{0,0,0,0};}
static cielo::ChiValue desiredCh(){return timeOK()?cielo::at(profile.ch,profile.chCount,minuteOfDay()):cielo::ChiValue{0,0,0};}

static void loadProfile(){
 profile=cielo96::ProfileImage{};
 profile.pvCount=cielo::pezverde_count;profile.chCount=cielo::chihiros_count;
 memcpy(profile.pv,cielo::pezverde,sizeof(cielo::pezverde));
 memcpy(profile.ch,cielo::chihiros,sizeof(cielo::chihiros));
 cielo96::profileSeal(profile);
 cielo96::ProfileImage a{},b{};
 bool va=false,vb=false;
 if(prefs.getBytesLength("curvesA")==sizeof(a)){
 prefs.getBytes("curvesA",&a,sizeof(a));va=cielo96::profileValid(a);
 }
 if(prefs.getBytesLength("curvesB")==sizeof(b)){
 prefs.getBytes("curvesB",&b,sizeof(b));vb=cielo96::profileValid(b);
 }
 if(va||vb){profile=va&&(!vb||int32_t(a.generation-b.generation)>0)?a:b;
  record("CURVAS_OK",String("NVS generacion=")+String(profile.generation));
 }else{
  bool corrupt=prefs.getBytesLength("curvesA")!=0 || prefs.getBytesLength("curvesB")!=0;
  profileRecovery=corrupt;
  record(corrupt?"CURVA_ERROR":"CURVAS_INICIALES",corrupt?"NVS invalida; Natural v4 en RAM":"Primera inicializacion; Natural v4 sin copia anterior");
 }
}
static bool saveProfile(const cielo96::ProfileImage& candidate){
 if(!cielo96::profileValid(candidate))return false;
 // Write inactive slot, read back, verify hash; keep previous valid slot untouched.
 const char*key=(candidate.generation&1)?"curvesA":"curvesB";
 if(prefs.putBytes(key,&candidate,sizeof(candidate))!=sizeof(candidate)){
  lastBackupError="NVS escritura incompleta";record("CURVA_ERROR",lastBackupError);return false;
 }
 cielo96::ProfileImage readback{};
 if(prefs.getBytes(key,&readback,sizeof(readback))!=sizeof(readback)||!cielo96::profileValid(readback)
    ||memcmp(&candidate,&readback,sizeof(readback))!=0){
  lastBackupError="NVS verificacion posterior fallo";record("CURVA_ERROR",lastBackupError);return false;
 }
 profile=candidate;profileRecovery=false;record("CURVA_GUARDADA",String("generacion=")+String(profile.generation));return true;
}
// Exact decimal parser: rejects empty, signs, trailing garbage and values outside bounds.
static bool positiveInt(const String&s,unsigned& n){
 if(!s.length())return false;uint32_t x=0;
 for(size_t i=0;i<s.length();i++){char c=s[i];if(c<'0'||c>'9')return false;x=x*10+(c-'0');if(x>1440)return false;}
 n=x;return true;
}
static bool parsePoints(const String&serialized,bool isPv,cielo96::ProfileImage&out){
 if(serialized.isEmpty() || serialized.length()>1700)return false;
 unsigned cols=isPv?5:4,count=0;
 size_t start=0;
 while(start<serialized.length()){
  size_t end=serialized.indexOf(';',start);if(end==size_t(-1))end=serialized.length();
  if(end==start||count>=64)return false;
  String line=serialized.substring(start,end);unsigned nums[5]{};size_t b=0;
  for(unsigned k=0;k<cols;k++){
   size_t e=line.indexOf(',',b);if(e==size_t(-1))e=line.length();
   if(k<cols-1 && e==line.length())return false;
   if(k==cols-1 && e!=line.length())return false;
   if(!positiveInt(line.substring(b,e),nums[k]))return false;b=e+1;
  }
  if(nums[0]>1440)return false;
  for(unsigned k=1;k<cols;k++)if(nums[k]>(isPv?(k==4?100:255):100))return false;
  if(isPv)out.pv[count]={(uint16_t)nums[0],(uint8_t)nums[1],(uint8_t)nums[2],(uint8_t)nums[3],(uint8_t)nums[4]};
  else out.ch[count]={(uint16_t)nums[0],(uint8_t)nums[1],(uint8_t)nums[2],(uint8_t)nums[3]};
  count++;start=end+1;
 }
 if(isPv)out.pvCount=count;else out.chCount=count;
 if(isPv){
  return cielo::valid(out.pv,count) && out.pv[count-1].minute==1440
         && out.pv[count-1].brightness==0;
 }
 return cielo::valid(out.ch,count) && out.ch[count-1].minute==1440
        && out.ch[count-1].r==0 && out.ch[count-1].g==0 && out.ch[count-1].b==0;
}
static void updateCurve(bool pv,bool reset){
 cielo96::ProfileImage candidate=profile;
 if(reset){
  if(pv){candidate.pvCount=cielo::pezverde_count;memcpy(candidate.pv,cielo::pezverde,sizeof(cielo::pezverde));}
  else{candidate.chCount=cielo::chihiros_count;memcpy(candidate.ch,cielo::chihiros,sizeof(cielo::chihiros));}
 }else if(!parsePoints(srv.arg("points"),pv,candidate))return fail(400,"Puntos invalidos; no se ha modificado ninguna curva");
 candidate.generation=profile.generation+1;cielo96::profileSeal(candidate);
 if(!saveProfile(candidate))return fail(507,"La curva anterior sigue disponible; error de grabacion");
 lastPvDesired=false;lastChDesired=false;ok();
}
static void statusJson(){
 auto p=desiredPv();auto c=desiredCh();bool w=WiFi.status()==WL_CONNECTED;
 String j;j.reserve(4500);
 j="{\"firmware\":\""+String(VERSION)+"\",\"ble\":"+String(bleReady?"true":"false")+",\"clockValid\":"+String(timeOK()?"true":"false");
 j+=",\"clock\":"+jsonQuote(dateTime())+",\"wifi\":"+(w?String("true"):String("false"));
 j+=",\"ip\":\"192.168.4.1\",\"homeIp\":"+jsonQuote(w?WiFi.localIP().toString():String(""));
 j+=",\"wifiStatus\":"+jsonQuote(w?"Router conectado":"No conectado")+",\"wifiConnecting\":false,\"wifiStage\":"+jsonQuote(w?"CONECTADA":"SIN_RED");
 j+=",\"manual\":"+String(pezManual?"true":"false")+",\"power\":"+String(p.brightness>0?"true":"false");
 j+=",\"rgb\":["+String(p.r)+","+String(p.g)+","+String(p.b)+"],\"brightness\":"+String(p.brightness);
 j+=",\"points\":"+jsonQuote(pointsPv())+",\"chiPoints\":"+jsonQuote(pointsCh());
 j+=",\"chActive\":"+String(chiCieloNight?"true":"false")+",\"chEvening\":"+String(chiCieloNight?"true":"false");
 j+=",\"chContinuous\":"+String(chiCieloNight?"true":"false")+",\"chStatus\":"+jsonQuote(chiAutoPending?"Restaurando AUTO (sin confirmacion fisica)":chiCieloNight?"Curva CIELO - transmision BLE sin confirmacion optica":"AUTO de My Chihiros solicitado")+"";
 j+=",\"chEveningStatus\":"+jsonQuote(chiCieloNight?"Curva CIELO":"AUTO solicitado")+",\"bleFailures\":"+String(bleResultErrors);
 j+=",\"txFrames\":"+String(devices[0].write_calls)+",\"txBatches\":"+String(devices[0].completed);
 j+=",\"bleError\":"+jsonQuote(lastBleError)+",\"bleLive\":false,\"storm\":false,\"stormStatus\":\"No disponible en R4 DEV\"";
 j+=",\"chFrames\":"+String(devices[1].write_calls)+",\"chFailures\":"+String(devices[1].errors);
 j+=",\"chLiveRgb\":["+String(c.r)+","+String(c.g)+","+String(c.b)+"],\"chLiveFailures\":0,\"chAutoAttempts\":"+String(chiAutoAttempted?1:0);
 j+=",\"otaPaired\":"+String(prefs.getBytesLength("ota_sha")==32?"true":"false")+"}";
 jsonSend(200,j);
}
static bool physicalOtaGrant();
static void diagnosticJson(){
 const auto&m=queue.metrics();bool wifi=WiFi.status()==WL_CONNECTED;
 uint32_t freeMem=heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
 uint32_t minimum=heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
 uint32_t largest=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
 bool badReset=abnormalReset(esp_reset_reason());
 bool workerUnavailable=bleReady && (millis()-cielo96::ble_last_heartbeat()>45000UL);
 bool warning=badReset||(!bleReady&&blePilotAttempted)||(!wifi&&!wifiApOn)||workerUnavailable||bleResultErrors||bleSlowCount||profileRecovery||m.soft_timeouts||otaError||!timeOK();
 String j;j.reserve(7200);
 j="{\"schema\":\"iwagumi-cielo-96-diagnostico-v2\",\"firmware\":"+jsonQuote(VERSION)+",\"generated_at\":"+jsonQuote(dateTime());
 j+=",\"uptime_sec\":"+String((millis()-bootMs)/1000UL)+",\"status\":"+jsonQuote(warning?"avisos_detectados":"sin_avisos_actuales");
 j+=",\"pilot\":{\"ble_start_requires_button\":true,\"ble_start_attempted\":"+String(blePilotAttempted?"true":"false")+",\"rescue_read_only\":"+String(rescueOpen?"true":"false")+",\"rescue_seconds\":"+String(rescueOpen?(rescueUntil-millis())/1000UL:0)+",\"private_ap_active\":"+String(apProtected?"true":"false")+"}";
 j+=",\"system\":{\"reset_reason\":"+String((unsigned)esp_reset_reason())+",\"reset_reason_name\":"+jsonQuote(resetReasonName(esp_reset_reason()))+",\"abnormal_reset\":"+String(badReset?"true":"false")+",\"boot_marked_healthy\":"+String(currentBootMarkedHealthy?"true":"false")+",\"watchdog_active\":null,\"last_phase\":\"NO_INSTRUMENTADA\"";
 j+=",\"heap_free\":"+String(freeMem)+",\"heap_minimum\":"+String(minimum)+",\"largest_heap_block\":"+String(largest);
 j+=",\"heap_metric\":\"MALLOC_CAP_INTERNAL\",\"filesystem_ok\":null,\"filesystem_errors\":null";
 j+=",\"boot_count\":"+String(previousBootCount+1)+",\"previous_boot_marked_clean\":"+String(priorBootClean?"true":"false");
 j+=",\"persistent_fault_count\":"+String(savedFailures)+",\"prior_fault_type\":"+jsonQuote(priorEvent)+",\"prior_fault_detail\":"+jsonQuote(priorEventDetail)+"}";
 j+=",\"network\":{\"home_wifi_connected\":"+String(wifi?"true":"false")+",\"ap_enabled\":"+String(wifiApOn?"true":"false");
 j+=",\"wifi_disconnects\":"+String(wifiDisconnects)+",\"wifi_connection_attempts\":"+String(wifiFailures)+",\"wifi_rssi_dbm\":"+String(wifi?WiFi.RSSI():0)+"}";
 j+=",\"lighting\":{\"pez_verde\":{\"mode\":"+jsonQuote(pezManual?"manual":"programa")+",\"tx_batches\":"+String(devices[0].completed);
 j+=",\"tx_frames\":"+String(devices[0].write_calls)+",\"errors\":"+String(devices[0].errors)+"},\"chihiros\":{\"cielo_night_enabled\":"+String(chiCieloNight?"true":"false");
 j+=",\"ble_active\":"+String(bleReady?"true":"false")+",\"auto_restore_pending\":"+String(chiAutoPending?"true":"false")+",\"tx_frames\":"+String(devices[1].write_calls)+",\"errors\":"+String(devices[1].errors)+"}}";
 j+=",\"bluetooth\":{\"worker_heartbeat_age_ms\":"+String(millis()-cielo96::ble_last_heartbeat());
 j+=",\"queued\":"+String(m.submitted)+",\"superseded\":"+String(m.superseded)+",\"failed\":"+String(m.failed);
 j+=",\"timeouts\":"+String(m.soft_timeouts)+",\"retries\":"+String(m.retries)+",\"max_call_ms\":"+String(bleDurationMax);
 j+=",\"slow_calls\":"+String(bleSlowCount);
 j+=",\"per_device\":{\"pez_verde\":{\"operations\":"+String(devices[0].completed)+",\"gatt_ok\":"+String(devices[0].gatt_ok)+",\"write_calls\":"+String(devices[0].write_calls)+",\"errors\":"+String(devices[0].errors)+",\"slow_calls\":"+String(devices[0].slow)+",\"last_ms\":"+String(devices[0].last_ms)+",\"max_ms\":"+String(devices[0].max_ms)+"}";
 j+=",\"chihiros\":{\"operations\":"+String(devices[1].completed)+",\"gatt_ok\":"+String(devices[1].gatt_ok)+",\"write_calls\":"+String(devices[1].write_calls)+",\"errors\":"+String(devices[1].errors)+",\"slow_calls\":"+String(devices[1].slow)+",\"last_ms\":"+String(devices[1].last_ms)+",\"max_ms\":"+String(devices[1].max_ms)+"}}";
 j+=",\"p95_ms\":{\"pez_verde\":"+String(latencyP95(0))+",\"chihiros\":"+String(latencyP95(1))+"}";
 j+=",\"worker_unresponsive\":"+String(workerUnavailable?"true":"false");
 j+=",\"note\":\"La escritura GATT completada no garantiza recepcion optica\"}";
 j+=",\"curves\":{\"generation\":"+String(profile.generation)+",\"points_pv\":"+String(profile.pvCount);
 j+=",\"points_ch\":"+String(profile.chCount)+",\"recovery_defaults\":"+String(profileRecovery?"true":"false")+",\"last_error\":"+jsonQuote(lastBackupError)+"}";
 j+=",\"updates\":{\"ota_local_configured\":"+String(CIELO_ENABLE_OTA?"true":"false")+",\"ota_in_progress\":"+String(otaBusy?"true":"false");
 j+=",\"last_ota_message\":"+jsonQuote(lastOtaError)+",\"github_check\":\"NO_USADO\",\"cloud\":\"DESACTIVADA\"}";
 j+=",\"recovery\":{\"ap_ssid\":\"CIELO96-CONFIG\",\"ap_ip\":\"192.168.4.1\",\"password_location\":\"SERIAL_USB_LOCAL\",\"always_on\":true";
 j+=",\"running_partition\":"+jsonQuote(otaRunningPartition)+",\"next_update_partition\":"+jsonQuote(otaNextPartition);
 j+=",\"physical_ota_grant\":"+String(physicalOtaGrant()?"true":"false")+",\"ota_enabled\":false}";
 j+=",\"event_retention\":\"36_RAM_y_ultimo_error_persistente_NVS\",\"events\":[";
 for(int i=0;i<eventCount;i++){
  int idx=(eventHead-eventCount+i+EVENT_MAX)%EVENT_MAX;
  if(i)j+=',';
  j+="{\"t_ms\":"+String(events[idx].elapsed)+",\"event\":"+jsonQuote(events[idx].label)+",\"detail\":"+jsonQuote(events[idx].detail)+"}";
 }
 j+="]}";
 jsonSend(200,j);
}
static void submitPv(cielo::PvValue p,uint32_t ttl=90000){
 if(!queue.submit(cielo::ble::Device::PezVerde,cielo::ble::Action::Apply,p.r,p.g,p.b,p.brightness,millis(),ttl))record("COLA_RECHAZADA","PV");
}
static void submitCh(cielo::ChiValue c,uint32_t ttl=90000){
 if(!queue.submit(cielo::ble::Device::Chihiros,cielo::ble::Action::Apply,c.r,c.g,c.b,0,millis(),ttl))record("COLA_RECHAZADA","CH");
}
static void requestAuto(){
 chiAutoPending=true;chiAutoAttempted=true;
 if(!queue.submit(cielo::ble::Device::Chihiros,cielo::ble::Action::RestoreAuto,0,0,0,0,millis(),90000))record("AUTO_RECHAZADO","Comando AUTO protegido");
}
static void manualPv(){
 if(srv.arg("mode")=="auto"){pezManual=false;lastPvDesired=false;submitPv(desiredPv());return ok();}
 if(!timeOK())return fail(503,"Reloj aun sin sincronizar");
 unsigned a[4]{};const char* k[]={"r","g","b","bright"};
 for(int i=0;i<4;i++){if(!positiveInt(srv.arg(k[i]),a[i])||a[i]>(i==3?100:255))return fail(400,"RGB o brillo incorrecto");}
 pezManual=true;pezManualUntil=millis()+20000UL;
 submitPv({uint8_t(a[0]),uint8_t(a[1]),uint8_t(a[2]),uint8_t(a[3])},20000);record("PV_MANUAL","20 segundos");ok();
}
static void manualCh(){
 if(!timeOK())return fail(503,"Reloj aun sin sincronizar");
 unsigned a[3]{};const char* k[]={"r","g","b"};
 for(int i=0;i<3;i++){if(!positiveInt(srv.arg(k[i]),a[i])||a[i]>100)return fail(400,"Canal Chihiros incorrecto");}
 chiManual=true;chiManualUntil=millis()+8000UL;
 submitCh({uint8_t(a[0]),uint8_t(a[1]),uint8_t(a[2])},8000);record("CH_MANUAL","8 segundos");ok();
}
static bool sha256String(const String&str,uint8_t dest[32]){return mbedtls_sha256_ret((const unsigned char*)str.c_str(),str.length(),dest,0)==0;}
static bool tokenEqual(){
 if(!CIELO_ENABLE_OTA)return false;
 if(prefs.getBytesLength("ota_sha")!=32)return false;
 uint8_t given[32],stored[32];if(!sha256String(srv.header("X-Cielo-Key"),given))return false;
 prefs.getBytes("ota_sha",stored,32);uint8_t diff=0;
 for(unsigned i=0;i<32;i++)diff|=stored[i]^given[i];return diff==0;
}
// Pilot firmware: 3-7 seconds then release starts BLE only when web is available.
// Holding >=8 sec enters temporary READ-ONLY rescue AP (no secret exposed).
static void physicalButtonTask(){
 const uint32_t now=millis();
 const bool held=digitalRead(ATOMS3_BUTTON_GPIO)==LOW;
 if(held && !buttonDownAt){buttonDownAt=now;physicalArmed=false;}
 if(held && buttonDownAt && !rescueOpen && now-buttonDownAt>=8000UL){
  rescueOpen=true;
  rescueUntil=now+120000UL;
  WiFi.softAPdisconnect(true);
  wifiApOn=WiFi.softAP("CIELO96-RESCATE");
  apProtected=false;
  record("RESCATE_ABIERTO",wifiApOn?"AP sin clave 120s y API solo lectura":"Fallo al levantar AP");
 }
 if(!held && buttonDownAt){
  const uint32_t heldMs=now-buttonDownAt;
  buttonDownAt=0;
  if(heldMs>=3000UL && heldMs<8000UL && !blePilotAttempted && !rescueOpen){
   blePilotRequested=true;record("BLE_PILOTO_SOLICITADO","Pulsacion GPIO41, se inicia una unica prueba BLE");
  }
 }
 if(rescueOpen && (int32_t)(now-rescueUntil)>=0){
  WiFi.softAPdisconnect(true);
  wifiApOn=WiFi.softAP("CIELO96-CONFIG",apPassword.c_str());
  apProtected=wifiApOn;rescueOpen=false;
  record("RESCATE_CERRADO","AP privado restaurado despues de 120s");
 }
}
static bool physicalOtaGrant(){return false;} // OTA deshabilitada en firmware de rescate
static void otaPair(){
 if(!CIELO_ENABLE_OTA)return fail(503,"OTA bloqueada en version experimental");
 if(prefs.getBytesLength("ota_sha")==32)return fail(409,"La clave OTA ya esta configurada");
 if(!physicalOtaGrant())return fail(403,"Mantener pulsado el boton fisico GPIO41 durante 5 segundos antes del emparejamiento");
 String s=srv.arg("key");if(s.length()<12||s.length()>96)return fail(400,"Clave OTA de 12 a 96 caracteres");
 uint8_t digest[32];if(!sha256String(s,digest)||prefs.putBytes("ota_sha",digest,32)!=32)return fail(507,"No se pudo guardar la clave");
 record("OTA_EMPAREJADA","Clave hasheada SHA256. No se registra en texto");ok();
}
static void startOtaResponse(){
 if(!CIELO_ENABLE_OTA)return fail(503,"OTA local desactivada por seguridad");
 if(!otaAllowed)return fail(403,"Clave OTA incorrecta");
 if(otaError)return fail(500,lastOtaError.c_str());
 if(!otaSuccess)return fail(500,"La imagen no se ha completado");
 srv.send(200,"text/plain","Firmware recibido y validado por Update; reiniciando");
 delay(400);ESP.restart();
}
static void otaUpload(){
 HTTPUpload&upload=srv.upload();
 if(upload.status==UPLOAD_FILE_START){
  otaAllowed=tokenEqual() && physicalOtaGrant();otaSuccess=false;otaError=false;otaBytes=0;
  if(!physicalOtaGrant())record("OTA_RECHAZADA","Falta confirmacion fisica del boton");
  if(!otaAllowed){lastOtaError="Sin clave correcta y autorizacion fisica";otaError=true;return;}
  if(!upload.filename.endsWith(".bin")||!Update.begin(UPDATE_SIZE_UNKNOWN,U_FLASH)){
   lastOtaError="BIN invalido o sin particion OTA disponible";otaError=true;return;
  }
  otaBusy=true;record("OTA_START","Carga manual autenticada");
 }else if(upload.status==UPLOAD_FILE_WRITE){
  if(!otaAllowed||otaError)return;
  if(Update.write(upload.buf,upload.currentSize)!=upload.currentSize){lastOtaError="Error de escritura OTA";otaError=true;Update.abort();}
  else otaBytes+=upload.currentSize;
 }else if(upload.status==UPLOAD_FILE_END){
  if(!otaAllowed||otaError)return;
  if(!Update.end(true)){lastOtaError="Firmware incompleto o incorrecto";otaError=true;Update.abort();}
  else{otaSuccess=true;lastOtaError="OTA verificada, reinicio pendiente";record("OTA_OK",String("bytes=")+String(otaBytes));}
  otaBusy=false;
 }else if(upload.status==UPLOAD_FILE_ABORTED){otaError=true;otaBusy=false;lastOtaError="OTA abortada";Update.abort();}
}
static void configureWifi(){
 String ssid=srv.arg("ssid"),pass=srv.arg("password");
 if(ssid.length()<1||ssid.length()>32||pass.length()>63)return fail(400,"SSID o password fuera de rango");
 if(prefs.putString("ssid",ssid)!=ssid.length())return fail(507,"No se pudo guardar la red");
 prefs.putString("wifi_pw",pass);storedSSID=ssid;storedPass=pass;
 record("WIFI_NUEVA","SSID actualizado (clave omitida)");
 lastWiFiAttempt=0;ok();
}
static void configureRoutes(){
 const char*keys[]={"X-Cielo-Key"};srv.collectHeaders(keys,1);
 srv.on("/",HTTP_GET,[]{++httpCount;srv.sendHeader("Cache-Control","no-store");srv.send_P(200,"text/html; charset=utf-8",CIELO_UI);});
 srv.on("/api/status",HTTP_GET,statusJson);
 srv.on("/api/diagnostico/maestro",HTTP_GET,[]{if(srv.hasArg("download"))srv.sendHeader("Content-Disposition","attachment; filename=\"CIELO_INFORME_MAESTRO.json\"");diagnosticJson();});
 srv.on("/api/service/report",HTTP_GET,diagnosticJson);
 srv.on("/api/service/status",HTTP_GET,[]{jsonSend(200,String("{\"version\":")+jsonQuote(VERSION)+",\"otaEnabled\":"+String(CIELO_ENABLE_OTA?"true":"false")+",\"otaPaired\":"+String(prefs.getBytesLength("ota_sha")==32?"true":"false")+",\"otaBusy\":"+String(otaBusy?"true":"false")+",\"otaBytes\":"+String(otaBytes)+"}");});
 srv.on("/api/clock",HTTP_POST,[]{if(rescueOpen)return fail(403,"Rescate solo lectura");unsigned epoch;if(!positiveInt(srv.arg("epoch"),epoch)){
   // epoch exceeds minute parser max, parse separately with strict digits
   String s=srv.arg("epoch");if(s.length()<10||s.length()>11)return fail(400,"Epoch incorrecto");
   for(unsigned i=0;i<s.length();i++)if(s[i]<'0'||s[i]>'9')return fail(400,"Epoch incorrecto");
   unsigned long long val=strtoull(s.c_str(),nullptr,10);if(val<1700000000ULL||val>4102444800ULL)return fail(400,"Epoch fuera de rango");
   struct timeval tv{(time_t)val,0};settimeofday(&tv,nullptr);record("HORA_SYNC","Reloj desde telefono");return ok();
  }return fail(400,"Epoch demasiado corto");});
 srv.on("/api/save",HTTP_POST,[]{if(rescueOpen)return fail(403,"Rescate solo lectura");updateCurve(true,false);});
 srv.on("/api/reset",HTTP_POST,[]{if(rescueOpen)return fail(403,"Rescate solo lectura");updateCurve(true,true);});
 srv.on("/api/chihiros/curve",HTTP_POST,[]{if(rescueOpen)return fail(403,"Rescate solo lectura");updateCurve(false,false);});
 srv.on("/api/chihiros/curve/reset",HTTP_POST,[]{if(rescueOpen)return fail(403,"Rescate solo lectura");updateCurve(false,true);});
 srv.on("/api/manual",HTTP_POST,[]{if(rescueOpen)return fail(403,"Rescate solo lectura");manualPv();});
 srv.on("/api/chihiros/manual",HTTP_POST,[]{if(rescueOpen)return fail(403,"Rescate solo lectura");manualCh();});
 srv.on("/api/chihiros/stop",HTTP_POST,[]{if(rescueOpen)return fail(403,"Rescate solo lectura");chiCieloNight=false;chiManual=false;requestAuto();ok();});
 srv.on("/api/chihiros/evening/off",HTTP_POST,[]{if(rescueOpen)return fail(403,"Rescate solo lectura");chiCieloNight=false;chiManual=false;requestAuto();ok();});
 srv.on("/api/chihiros/evening/on",HTTP_POST,[]{if(rescueOpen)return fail(403,"Rescate solo lectura");chiCieloNight=true;chiManual=false;chiAutoPending=false;chiAutoAttempted=false;lastChDesired=false;ok();});
 srv.on("/api/chihiros/live/stop",HTTP_POST,[]{if(rescueOpen)return fail(403,"Rescate solo lectura");chiCieloNight=false;chiManual=false;requestAuto();ok();});
 srv.on("/api/wifi",HTTP_POST,[]{if(rescueOpen)return fail(403,"Rescate solo lectura");configureWifi();});
 srv.on("/api/wifi/forget",HTTP_POST,[]{if(rescueOpen)return fail(403,"Rescate solo lectura");prefs.remove("ssid");prefs.remove("wifi_pw");storedSSID="";storedPass="";WiFi.disconnect(false,false);ok();});
 srv.on("/api/ota/pair",HTTP_POST,[]{if(rescueOpen)return fail(403,"Rescate solo lectura");otaPair();});
 srv.on("/api/ota/upload",HTTP_POST,[]{if(rescueOpen)return fail(403,"Rescate solo lectura");startOtaResponse();},otaUpload);
 const char*unsupported[]={"/api/chihiros/test","/api/storm/start","/api/storm/stop",
   "/api/chihiros/aurora931/apply","/api/chihiros/aurora931/restore",
   "/api/pez/cielo931/apply","/api/pez/cielo931/restore","/api/pez/cielo94/apply","/api/pez/cielo94/restore",
   "/api/updates/check","/api/updates/install","/api/updates/status",
   "/api/diagnostico/nube/configurar","/api/diagnostico/nube/desactivar","/api/diagnostico/nube/estado","/api/diagnostico/nube/sincronizar"};
 for(const char*path:unsupported){srv.on(path,HTTP_ANY,[]{fail(501,"Funcion todavia no implementada en R4; no se ha modificado la iluminacion");});}
 srv.onNotFound([](){fail(404,"Ruta no disponible en R4");});
 srv.begin();
}
static void wifiTask(){
 const uint32_t now=millis();
 wl_status_t w=WiFi.status();
 if(lastWifiStatus==WL_CONNECTED && w!=WL_CONNECTED){wifiDisconnects++;record("WIFI_CAIDA","STA desconectada; AP de rescate continua activo");}
 lastWifiStatus=w;
 if(!wifiApOn && now-bootMs>5000){
  wifiApOn=WiFi.softAP("CIELO96-CONFIG",apPassword.c_str());
  apProtected=wifiApOn;
  record("AP_RECUPERACION",String("192.168.4.1; WPA2; disponible=")+String(wifiApOn));
 }
 if(w==WL_CONNECTED)return;
 if(storedSSID.length() && now-lastWiFiAttempt>30000UL){
  lastWiFiAttempt=now;WiFi.begin(storedSSID.c_str(),storedPass.c_str());wifiFailures++;
 }

}
static void lightingTask(){
 uint32_t now=millis();
 if(!timeOK())return;
 // Timeouts of manual previews use subtraction safe under millis wraparound.
 if(pezManual && (int32_t)(now-pezManualUntil)>=0){pezManual=false;lastPvDesired=false;record("PV_MANUAL_FIN","Volver a curva");}
 if(chiManual && (int32_t)(now-chiManualUntil)>=0){chiManual=false;lastChDesired=false;record("CH_MANUAL_FIN","Volver a curva o AUTO");}
 if(now-lastScheduleAt<12000UL)return;lastScheduleAt=now;
 const int minute=minuteOfDay();
 if(!pezManual){
  auto p=desiredPv();
  if(!lastPvDesired||memcmp(&p,&prevPv,sizeof(p))!=0||now-lastPvDesiredAt>300000UL){
   submitPv(p);prevPv=p;lastPvDesired=true;lastPvDesiredAt=now;
  }
 }
 // 14:30 to 23:00: CIELO drives RGB. From 23:00: return Chihiros to My Chihiros AUTO.
 const bool within=(minute>=870&&minute<1380);
 if(chiCieloNight && within && !chiManual){
  auto c=desiredCh();
  if(!lastChDesired||memcmp(&c,&prevCh,sizeof(c))!=0||now-lastChDesiredAt>300000UL){
   submitCh(c);prevCh=c;lastChDesired=true;lastChDesiredAt=now;chiAutoPending=false;chiAutoAttempted=false;
  }
 }else if(!within && !chiAutoAttempted){
  requestAuto();lastChDesired=false;
 }
}
static void bleTick(){
 uint32_t now=millis();cielo96::BleResult res{};
 while(cielo96::ble_result(res)){
  uint8_t device=res.device==1?1:0;
  DevTelemetry& t=devices[device];t.completed++;t.write_calls+=res.write_calls;t.last_ms=res.duration_ms;
  latencyHistory[device][latencyCount[device]++%32]=res.duration_ms;
  if(res.duration_ms>t.max_ms)t.max_ms=res.duration_ms;
  if(res.duration_ms>3500)t.slow++;
  if(res.success)t.gatt_ok++;else t.errors++;
  queue.complete(res.sequence,res.success,now);bleResultCount++;
  if(res.duration_ms>bleDurationMax)bleDurationMax=res.duration_ms;
  if(res.duration_ms>3500UL){bleSlowCount++;record("BLE_LENTO",String(res.device?"CH ":"PV ")+String(res.duration_ms)+"ms");}
  if(!res.success){bleResultErrors++;lastBleError=String(res.device?"CH ":"PV ")+String(res.reason);record("BLE_ERROR",lastBleError);}
  else if(res.device==1 && chiAutoPending){chiAutoPending=false;record("CH_AUTO","GATT completado, efecto optico no verificado");}
 }
 queue.tick(now);
 const uint32_t timeoutNow=queue.metrics().soft_timeouts;
 if(timeoutNow>bleSoftTimeouts){bleSoftTimeouts=timeoutNow;record("BLE_SOFT_TIMEOUT","Web sigue activo; tarea BLE no liberada artificialmente");}
 cielo::ble::Command cmd{};
 if(queue.start_next(now,cmd)){
  if(!cielo96::ble_dispatch(cmd)){
    queue.complete(cmd.sequence,false,now);record("BLE_COLA_LLENA","No se envio comando al worker");
  }
 }
}
void setup(){
 bootMs=millis();Serial.begin(115200);delay(100);
 Serial.printf("IWAGUMI CIELO %s | ATOMS3-Lite | build experimental\n",VERSION);
 setenv("TZ","CET-1CEST,M3.5.0,M10.5.0/3",1);tzset();
 prefs.begin("cielo96",false);
 previousBootCount=prefs.getUInt("boots",0);
 priorEvent=prefs.getString("fault_type","ninguno");
 priorEventDetail=prefs.getString("fault_text","ninguno");
 savedFailures=prefs.getUInt("fault_count",0);
 priorBootClean=prefs.getBool("clean",false);
 // One small write per boot; never on minute timer.
 prefs.putUInt("boots",previousBootCount+1);
 prefs.putBool("clean",false);
 const esp_partition_t *running=esp_ota_get_running_partition();
 const esp_partition_t *next=esp_ota_get_next_update_partition(nullptr);
 otaRunningPartition=running?running->label:"desconocida";
 otaNextPartition=next?next->label:"NO_DISPONIBLE";
 loadProfile();storedSSID=prefs.getString("ssid",CIELO_WIFI_SSID);storedPass=prefs.getString("wifi_pw",CIELO_WIFI_PASSWORD);
 // Recover credentials from the original 9.5.3 namespace, retained by application-only OTA.
 // Verified on owner's private 8MB flash: namespace "iwagumi-rgb", keys homeSsid/homePass.
 // Never embed passwords in firmware, logs, source repository, report or response.
 if(storedSSID.isEmpty()){
  Preferences legacy;
  if(legacy.begin("iwagumi-rgb",true)){
   String oldSsid=legacy.getString("homeSsid","");
   String oldPass=legacy.getString("homePass","");
   legacy.end();
   if(oldSsid.length()>0 && oldSsid.length()<=32 && oldPass.length()<=63){
    if(prefs.putString("ssid",oldSsid)==oldSsid.length()){
     prefs.putString("wifi_pw",oldPass);
     storedSSID=oldSsid;storedPass=oldPass;
     record("WIFI_MIGRADA","Configuracion local 9.5.3 reutilizada sin mostrar claves");
    }
   }
  }
 }
 // Emergency AP secret is created from hardware randomness on first boot,
 // persisted in NVS, printed ONLY on the physical serial port.
 apPassword=prefs.getString("ap_secret","");
 if(apPassword.length()<12){
   static const char alphabet[]="ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
   apPassword="";
   for(int i=0;i<14;i++)apPassword+=alphabet[esp_random()%(sizeof(alphabet)-1)];
   prefs.putString("ap_secret",apPassword);
 }
 pinMode(ATOMS3_BUTTON_GPIO,INPUT_PULLUP);
 statusPixel.begin();statusPixel.setBrightness(32);statusPixel.clear();statusPixel.show();
 WiFi.mode(WIFI_AP_STA);WiFi.setSleep(false);WiFi.setAutoReconnect(false);
 WiFi.setHostname("iwagumi-cielo96");
 wifiApOn=WiFi.softAP("CIELO96-CONFIG",apPassword.c_str());
 apProtected=wifiApOn;
 if(storedSSID.length()){WiFi.begin(storedSSID.c_str(),storedPass.c_str());lastWiFiAttempt=millis();}
 configTime(0,0,"pool.ntp.org","time.google.com");
 bleReady=false; // Do not start BLE until explicit physical authorization.
 record("INICIO",String("reset=")+String((unsigned)esp_reset_reason())+" ("+resetReasonName(esp_reset_reason())+") BLE="+String(bleReady));
 if(abnormalReset(esp_reset_reason()))record("REINICIO_ANOMALO",resetReasonName(esp_reset_reason()));
 Serial.printf("WIFI de rescate si fuera necesaria: CIELO96-CONFIG | clave: %s\n",apPassword.c_str());
 configureRoutes();
 record("WEB_PRIMERO","HTTP y AP de recuperación inicializados; Bluetooth en espera");
}
void loop(){
 if(!currentBootMarkedHealthy && millis()-bootMs>=180000UL){
  currentBootMarkedHealthy=true;
  prefs.putBool("clean",true);
  record("ARRANQUE_ESTABLE","180 segundos sin reiniciar; NO implica que BLE funcione correctamente");
 }
 srv.handleClient();
 physicalButtonTask();
 wifiTask();
 if(blePilotRequested && !blePilotAttempted && !rescueOpen){
  blePilotRequested=false;blePilotAttempted=true;
  record("BLE_PILOTO_INICIO","Se prueba Bluetooth con HTTP ya activo");
  bleReady=cielo96::ble_begin();
  record(bleReady?"BLE_PILOTO_PREPARADO":"BLE_PILOTO_ERROR",bleReady?"Tarea BLE creada":"No se pudo crear tarea BLE");
 }
 if(bleReady && !rescueOpen){lightingTask();bleTick();}
 statusLedTick();
 delay(3);
}
