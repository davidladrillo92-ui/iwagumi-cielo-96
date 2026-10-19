// IWAGUMI CIELO 9.6 R4 development firmware
// ESP32-S3 / ATOMS3 Lite C124. Source implementation; hardware NOT YET VERIFIED.
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <Update.h>
#include <time.h>
#include <sys/time.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_attr.h>
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

static constexpr const char* VERSION="9.6.0-R4.3-PILOTO";
static WebServer srv(80);
static Preferences prefs;
static cielo96::ProfileImage profile;
static cielo::ble::Scheduler queue(3500,250,2000);
static bool bleReady=false, profileRecovery=false, wifiApOn=false;
static bool pezManual=false,chiManual=false,chiCieloNight=true,chiAutoPending=false;
static uint32_t pezManualUntil=0,chiManualUntil=0;
static uint32_t lastScheduleAt=0,lastWiFiAttempt=0,lastStatusAt=0,otaBytes=0,otaRestarts=0;
static uint32_t lastPvDesiredAt=0,lastChDesiredAt=0;
static uint32_t httpCount=0,bleResultCount=0,bleResultErrors=0,bleSoftTimeouts=0;
static uint32_t bleSlowCount=0,bleDurationMax=0,wifiFailures=0;
struct DevTelemetry {uint32_t completed=0,gatt_ok=0,write_calls=0,errors=0,slow=0,last_ms=0,max_ms=0;};
static DevTelemetry devices[2];
static uint32_t latencyMs[2][32]{},latencyCount[2]{};
static uint32_t p95(int dev){
 uint32_t sorted[32]{};unsigned n=latencyCount[dev]<32?latencyCount[dev]:32;
 if(!n)return 0;
 for(unsigned i=0;i<n;i++)sorted[i]=latencyMs[dev][i];
 for(unsigned i=1;i<n;i++){uint32_t v=sorted[i];int j=int(i)-1;while(j>=0&&sorted[j]>v){sorted[j+1]=sorted[j];--j;}sorted[j+1]=v;}
 return sorted[(95*n+99)/100-1];
}
RTC_NOINIT_ATTR static uint32_t rtcCieloMagic,rtcBlePhase,rtcBootCount;
static uint32_t previousPhase=0,persistentBoots=0,lastStoredFault=0;
static String persistedFault;
static void saveCriticalIncident(const String&message){
 if(lastStoredFault && millis()-lastStoredFault<3600000UL)return;
 lastStoredFault=millis();
 String safe=message.substring(0,92);prefs.putString("last_fault",safe);persistedFault=safe;
}
 // PV and Chihiros. GATT acceptance is not an optical ACK.
static bool lastPvDesired=false,lastChDesired=false,chiAutoAttempted=false;
static cielo::PvValue prevPv{};
static cielo::ChiValue prevCh{};
static bool otaBusy=false,otaAllowed=false,otaSuccess=false,otaError=false;
static mbedtls_sha256_context otaHash;
static bool otaHashStarted=false;
static uint8_t otaExpected[32]{};
static uint8_t otaFailures=0;
static uint32_t otaBlockedUntil=0;
static bool parseSha256(const String&s,uint8_t out[32]){
 if(s.length()!=64)return false;
 for(int i=0;i<32;i++){
  char a=s[2*i],b=s[2*i+1];
  auto hex=[](char x)->int{return x>='0'&&x<='9'?x-'0':x>='a'&&x<='f'?x-'a'+10:x>='A'&&x<='F'?x-'A'+10:-1;};
  int hi=hex(a),lo=hex(b);if(hi<0||lo<0)return false;out[i]=(hi<<4)|lo;
 }return true;
}
static void endOtaHash(){if(otaHashStarted){mbedtls_sha256_free(&otaHash);otaHashStarted=false;}}

static String lastBleError="ninguno",lastBackupError="ninguno",lastOtaError="Sin actualizaciones";
static String storedSSID,storedPass,apPassword;
static bool wifiMigrated=false,mdnsStarted=false,rescueOpen=false,safeBoot=false;
static uint32_t physicalUntil=0,rescueUntil=0,buttonStarted=0;
static bool physicalButtonPrev=false,buttonArmed=false,buttonRescueArmed=false;
static bool physicalWindow(){return physicalUntil!=0 && int32_t(physicalUntil-millis())>0;}
static uint32_t physicalSeconds(){return physicalWindow()?(physicalUntil-millis()+999)/1000:0;}
static void recoverOldWifi(){
 if(storedSSID.length())return;
 Preferences old;
 if(!old.begin("iwagumi-rgb",true))return;
 String ssid=old.getString("homeSsid",""),pass=old.getString("homePass","");
 old.end();
 if(!ssid.length()||ssid.length()>32||pass.length()>63)return;
 storedSSID=ssid;storedPass=pass;wifiMigrated=true;
 prefs.putString("ssid",storedSSID);prefs.putString("wifi_pw",storedPass);
 record("WIFI_MIGRADA","Red 9.5.3 migrada internamente sin mostrar secretos");
}
static uint32_t bootMs=0;
static uint32_t lastEventId=0;
static constexpr int EVENT_MAX=36;
struct Event{uint32_t elapsed;char label[22];char detail[82];};
static Event events[EVENT_MAX];static int eventCount=0,eventHead=0;

static void record(const char*name,const String& detail){
 Event&e=events[eventHead];e.elapsed=millis()-bootMs;
 snprintf(e.label,sizeof(e.label),"%s",name);
 snprintf(e.detail,sizeof(e.detail),"%s",detail.c_str());
 eventHead=(eventHead+1)%EVENT_MAX;if(eventCount<EVENT_MAX)eventCount++;lastEventId++;
 Serial.printf("CIELO EVENT %s %s\n",name,detail.c_str());
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
 j+=",\"otaPaired\":"+String(prefs.getBytesLength("ota_sha")==32?"true":"false")+",\"otaEnabled\":"+String(CIELO_ENABLE_OTA?"true":"false")+",\"physicalSeconds\":"+String(physicalSeconds())+",\"wifiMigrated\":"+String(wifiMigrated?"true":"false")+",\"safeBoot\":"+String(safeBoot?"true":"false")+"}";
 jsonSend(200,j);
}
static void diagnosticJson(){
 const auto&m=queue.metrics();bool wifi=WiFi.status()==WL_CONNECTED;
 uint32_t freeMem=heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
 uint32_t minimum=heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
 uint32_t largest=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
 bool warning=bleResultErrors||bleSlowCount||profileRecovery||m.soft_timeouts||otaError||!timeOK();
 String j;j.reserve(7200);
 j="{\"schema\":\"iwagumi-cielo-96-diagnostico-v2\",\"firmware\":"+jsonQuote(VERSION)+",\"generated_at\":"+jsonQuote(dateTime());
 j+=",\"uptime_sec\":"+String((millis()-bootMs)/1000UL)+",\"status\":"+jsonQuote(warning?"avisos_detectados":"sin_avisos_actuales");
 j+=",\"system\":{\"reset_reason\":"+String((unsigned)esp_reset_reason())+",\"watchdog_active\":null,\"last_phase\":\"NO_INSTRUMENTADA\"";
 j+=",\"heap_free\":"+String(freeMem)+",\"heap_minimum\":"+String(minimum)+",\"largest_heap_block\":"+String(largest);
 j+=",\"heap_metric\":\"MALLOC_CAP_INTERNAL\",\"filesystem_ok\":null,\"filesystem_errors\":null,\"boot_count\":"+String(persistentBoots)+",\"previous_runtime_phase\":"+String(previousPhase)+",\"rtc_phase_current\":"+String(rtcBlePhase)+",\"last_stored_fault\":"+jsonQuote(persistedFault)+"}";
 j+=",\"network\":{\"home_wifi_connected\":"+String(wifi?"true":"false")+",\"ap_enabled\":"+String(wifiApOn?"true":"false");
 j+=",\"wifi_disconnects\":null,\"wifi_connection_attempts\":"+String(wifiFailures)+",\"wifi_migrated_from_953\":"+String(wifiMigrated?"true":"false")+",\"mdns_started\":"+String(mdnsStarted?"true":"false")+",\"recovery_ap_open\":"+String(rescueOpen?"true":"false")+",\"physical_unlock_remaining_sec\":"+String(physicalSeconds())+",\"wifi_rssi_dbm\":"+String(wifi?WiFi.RSSI():0)+"}";
 j+=",\"lighting\":{\"pez_verde\":{\"mode\":"+jsonQuote(pezManual?"manual":"programa")+",\"tx_batches\":"+String(devices[0].completed);
 j+=",\"tx_frames\":"+String(devices[0].write_calls)+",\"errors\":"+String(devices[0].errors)+"},\"chihiros\":{\"cielo_night_enabled\":"+String(chiCieloNight?"true":"false");
 j+=",\"ble_active\":"+String(bleReady?"true":"false")+",\"auto_restore_pending\":"+String(chiAutoPending?"true":"false")+",\"tx_frames\":"+String(devices[1].write_calls)+",\"errors\":"+String(devices[1].errors)+"}}";
 j+=",\"bluetooth\":{\"worker_heartbeat_age_ms\":"+String(millis()-cielo96::ble_last_heartbeat());
 j+=",\"queued\":"+String(m.submitted)+",\"superseded\":"+String(m.superseded)+",\"failed\":"+String(m.failed);
 j+=",\"timeouts\":"+String(m.soft_timeouts)+",\"retries\":"+String(m.retries)+",\"max_call_ms\":"+String(bleDurationMax);
 j+=",\"slow_calls\":"+String(bleSlowCount);
 j+=",\"per_device\":{\"pez_verde\":{\"operations\":"+String(devices[0].completed)+",\"gatt_ok\":"+String(devices[0].gatt_ok)+",\"write_calls\":"+String(devices[0].write_calls)+",\"errors\":"+String(devices[0].errors)+",\"slow_calls\":"+String(devices[0].slow)+",\"last_ms\":"+String(devices[0].last_ms)+",\"max_ms\":"+String(devices[0].max_ms)+"}";
 j+=",\"chihiros\":{\"operations\":"+String(devices[1].completed)+",\"gatt_ok\":"+String(devices[1].gatt_ok)+",\"write_calls\":"+String(devices[1].write_calls)+",\"errors\":"+String(devices[1].errors)+",\"slow_calls\":"+String(devices[1].slow)+",\"last_ms\":"+String(devices[1].last_ms)+",\"max_ms\":"+String(devices[1].max_ms)+"}}";
 j+=",\"p95_ms\":{\"pez_verde\":"+String(p95(0))+",\"chihiros\":"+String(p95(1))+"}";
 j+=",\"note\":\"La escritura GATT completada no garantiza recepcion optica\"}";
 j+=",\"curves\":{\"generation\":"+String(profile.generation)+",\"points_pv\":"+String(profile.pvCount);
 j+=",\"points_ch\":"+String(profile.chCount)+",\"recovery_defaults\":"+String(profileRecovery?"true":"false")+",\"last_error\":"+jsonQuote(lastBackupError)+"}";
 j+=",\"updates\":{\"ota_local_configured\":"+String(CIELO_ENABLE_OTA?"true":"false")+",\"ota_in_progress\":"+String(otaBusy?"true":"false");
 j+=",\"last_ota_message\":"+jsonQuote(lastOtaError)+",\"github_check\":\"NO_USADO\",\"cloud\":\"DESACTIVADA\"}";
 j+=",\"event_retention\":\"RAM_36_SOLO_ARRANQUE_ACTUAL\",\"events\":[";
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
 chiManual=true;chiManualUntil=millis()+20000UL;chiAutoAttempted=false;
 submitCh({uint8_t(a[0]),uint8_t(a[1]),uint8_t(a[2])},20000);record("CH_MANUAL","20 segundos; retorno programado");ok();
}
static bool sha256String(const String&str,uint8_t dest[32]){return mbedtls_sha256_ret((const unsigned char*)str.c_str(),str.length(),dest,0)==0;}
static bool tokenEqual(){
 if(!CIELO_ENABLE_OTA)return false;
 if(prefs.getBytesLength("ota_sha")!=32)return false;
 uint8_t given[32],stored[32];if(!sha256String(srv.header("X-Cielo-Key"),given))return false;
 prefs.getBytes("ota_sha",stored,32);uint8_t diff=0;
 for(unsigned i=0;i<32;i++)diff|=stored[i]^given[i];return diff==0;
}
static void otaPair(){
 if(!CIELO_ENABLE_OTA)return fail(503,"OTA bloqueada en version experimental");
 if(prefs.getBytesLength("ota_sha")==32)return fail(409,"Clave OTA ya configurada");
 if(!physicalWindow())return fail(403,"Emparejamiento requiere boton fisico pulsado 3 segundos");
 // El emparejamiento está autorizado mediante presencia física, no un plazo tras arrancar.
 String s=srv.arg("key");if(s.length()<12||s.length()>96)return fail(400,"Clave OTA de 12 a 96 caracteres");
 uint8_t digest[32];if(!sha256String(s,digest)||prefs.putBytes("ota_sha",digest,32)!=32)return fail(507,"No se pudo guardar la clave");
 record("OTA_EMPAREJADA","Clave hasheada SHA256. No se registra en texto");ok();
}
static void startOtaResponse(){
 if(!CIELO_ENABLE_OTA)return fail(503,"OTA local desactivada por seguridad");
 if(!otaAllowed)return fail(403,"Clave OTA incorrecta o falta autorización física");
 if(otaError)return fail(500,lastOtaError.c_str());
 if(!otaSuccess)return fail(500,"La imagen no se ha completado");
 srv.send(200,"text/plain","Firmware recibido y validado por Update; reiniciando");
 delay(400);ESP.restart();
}
static void otaUpload(){
 HTTPUpload&upload=srv.upload();
 if(upload.status==UPLOAD_FILE_START){
  otaAllowed=false;otaSuccess=false;otaError=false;otaBytes=0;endOtaHash();
  if(otaBlockedUntil && int32_t(otaBlockedUntil-millis())>0){
   lastOtaError="OTA temporalmente bloqueada";otaError=true;return;
  }
  if(!physicalWindow()||!tokenEqual()){
   lastOtaError="Se exige boton fisico 3 segundos y clave local correcta";otaError=true;
   if(++otaFailures>=5){otaBlockedUntil=millis()+60000;otaFailures=0;}return;
  }
  otaFailures=0;
  if(!upload.filename.endsWith(".bin")||!parseSha256(srv.header("X-Cielo-SHA256"),otaExpected)){
   lastOtaError="BIN o huella SHA256 incorrectos";otaError=true;return;
  }
  if(!Update.begin(UPDATE_SIZE_UNKNOWN,U_FLASH)){lastOtaError="Particion OTA no disponible";otaError=true;return;}
  mbedtls_sha256_init(&otaHash);otaHashStarted=true;
  if(mbedtls_sha256_starts_ret(&otaHash,0)!=0){
   lastOtaError="Error inicializando SHA256";otaError=true;Update.abort();endOtaHash();return;
  }
  otaAllowed=true;otaBusy=true;record("OTA_INICIO","BIN local, presencia fisica, PIN y SHA");
 }else if(upload.status==UPLOAD_FILE_WRITE){
  if(!otaAllowed||otaError)return;
  if(otaBytes==0 && (upload.currentSize==0||upload.buf[0]!=0xe9)){
   lastOtaError="No es una imagen ESP32";otaError=true;Update.abort();endOtaHash();otaBusy=false;return;
  }
  if(otaBytes+upload.currentSize>3342336UL ||
     mbedtls_sha256_update_ret(&otaHash,upload.buf,upload.currentSize)!=0 ||
     Update.write(upload.buf,upload.currentSize)!=upload.currentSize){
   lastOtaError="Error de tamaño, SHA256 o escritura";otaError=true;Update.abort();endOtaHash();otaBusy=false;return;
  }
  otaBytes+=upload.currentSize;
 }else if(upload.status==UPLOAD_FILE_END){
  if(!otaAllowed||otaError)return;
  uint8_t digest[32]{};
  if(!otaHashStarted||mbedtls_sha256_finish_ret(&otaHash,digest)!=0){
   lastOtaError="Error final SHA256";otaError=true;Update.abort();endOtaHash();otaBusy=false;return;
  }
  endOtaHash();uint8_t diff=0;
  for(int i=0;i<32;i++)diff|=digest[i]^otaExpected[i];
  if(diff||otaBytes<50000UL){
   lastOtaError="BIN rechazado: SHA256 no coincide o tamaño pequeño";
   otaError=true;Update.abort();otaBusy=false;record("OTA_RECHAZADA","BIN no supera integridad");return;
  }
  if(!Update.end(true)){lastOtaError="Imagen OTA invalida";otaError=true;Update.abort();}
  else{otaSuccess=true;lastOtaError="BIN SHA256 comprobado y OTA aceptada";record("OTA_VERIFICADA",String("bytes=")+String(otaBytes));}
  otaBusy=false;
 }else if(upload.status==UPLOAD_FILE_ABORTED){
  otaError=true;otaBusy=false;lastOtaError="Subida interrumpida";Update.abort();endOtaHash();
  record("OTA_ABORTADA","Firmware activo sin modificar");
 }
}
static void configureWifi(){
 if(!physicalWindow())return fail(403,"WiFi requiere mantener el boton 3 segundos");
 String ssid=srv.arg("ssid"),pass=srv.arg("password");
 if(ssid.length()<1||ssid.length()>32||pass.length()>63)return fail(400,"SSID o password fuera de rango");
 if(prefs.putString("ssid",ssid)!=ssid.length())return fail(507,"No se pudo guardar la red");
 prefs.putString("wifi_pw",pass);storedSSID=ssid;storedPass=pass;
 record("WIFI_NUEVA","SSID actualizado (clave omitida)");
 lastWiFiAttempt=0;ok();
}
static void configureRoutes(){
 const char*keys[]={"X-Cielo-Key","X-Cielo-SHA256"};srv.collectHeaders(keys,2);
 srv.on("/",HTTP_GET,[]{++httpCount;srv.sendHeader("Cache-Control","no-store");srv.send_P(200,"text/html; charset=utf-8",CIELO_UI);});
 srv.on("/api/status",HTTP_GET,statusJson);
 srv.on("/api/diagnostico/maestro",HTTP_GET,[]{if(srv.hasArg("download"))srv.sendHeader("Content-Disposition","attachment; filename=\"CIELO_INFORME_MAESTRO.json\"");diagnosticJson();});
 srv.on("/api/service/report",HTTP_GET,diagnosticJson);
 srv.on("/api/service/status",HTTP_GET,[]{
 jsonSend(200,String("{\"version\":")+jsonQuote(VERSION)
  +",\"otaEnabled\":"+String(CIELO_ENABLE_OTA?"true":"false")
  +",\"otaPaired\":"+String(prefs.getBytesLength("ota_sha")==32?"true":"false")
  +",\"otaBusy\":"+String(otaBusy?"true":"false")
  +",\"otaBytes\":"+String(otaBytes)
  +",\"physicalSeconds\":"+String(physicalSeconds())
  +",\"recoveryApOpen\":"+String(rescueOpen?"true":"false")+"}");
 });
 srv.on("/api/clock",HTTP_POST,[]{unsigned epoch;if(!positiveInt(srv.arg("epoch"),epoch)){
   // epoch exceeds minute parser max, parse separately with strict digits
   String s=srv.arg("epoch");if(s.length()<10||s.length()>11)return fail(400,"Epoch incorrecto");
   for(unsigned i=0;i<s.length();i++)if(s[i]<'0'||s[i]>'9')return fail(400,"Epoch incorrecto");
   unsigned long long val=strtoull(s.c_str(),nullptr,10);if(val<1700000000ULL||val>4102444800ULL)return fail(400,"Epoch fuera de rango");
   struct timeval tv{(time_t)val,0};settimeofday(&tv,nullptr);record("HORA_SYNC","Reloj desde telefono");return ok();
  }return fail(400,"Epoch demasiado corto");});
 srv.on("/api/save",HTTP_POST,[]{updateCurve(true,false);});
 srv.on("/api/reset",HTTP_POST,[]{updateCurve(true,true);});
 srv.on("/api/chihiros/curve",HTTP_POST,[]{updateCurve(false,false);});
 srv.on("/api/chihiros/curve/reset",HTTP_POST,[]{updateCurve(false,true);});
 srv.on("/api/manual",HTTP_POST,manualPv);
 srv.on("/api/chihiros/manual",HTTP_POST,manualCh);
 srv.on("/api/chihiros/stop",HTTP_POST,[]{chiCieloNight=false;chiManual=false;requestAuto();ok();});
 srv.on("/api/chihiros/evening/off",HTTP_POST,[]{chiCieloNight=false;chiManual=false;requestAuto();ok();});
 srv.on("/api/chihiros/evening/on",HTTP_POST,[]{chiCieloNight=true;chiManual=false;chiAutoPending=false;chiAutoAttempted=false;lastChDesired=false;ok();});
 srv.on("/api/chihiros/live/stop",HTTP_POST,[]{chiCieloNight=false;chiManual=false;requestAuto();ok();});
 srv.on("/api/wifi",HTTP_POST,configureWifi);
 srv.on("/api/wifi/forget",HTTP_POST,[]{if(!physicalWindow())return fail(403,"WiFi requiere boton");prefs.remove("ssid");prefs.remove("wifi_pw");storedSSID="";storedPass="";WiFi.disconnect(false,false);ok();});
 srv.on("/api/ota/pair",HTTP_POST,otaPair);
 srv.on("/api/ota/upload",HTTP_POST,startOtaResponse,otaUpload);
 const char*unsupported[]={"/api/chihiros/test","/api/storm/start","/api/storm/stop",
   "/api/chihiros/aurora931/apply","/api/chihiros/aurora931/restore",
   "/api/pez/cielo931/apply","/api/pez/cielo931/restore","/api/pez/cielo94/apply","/api/pez/cielo94/restore",
   "/api/updates/check","/api/updates/install","/api/updates/status",
   "/api/diagnostico/nube/configurar","/api/diagnostico/nube/desactivar","/api/diagnostico/nube/estado","/api/diagnostico/nube/sincronizar"};
 for(const char*path:unsupported){srv.on(path,HTTP_ANY,[]{fail(501,"Funcion todavia no implementada en R4; no se ha modificado la iluminacion");});}
 srv.onNotFound([](){fail(404,"Ruta no disponible en R4");});
 srv.begin();
}
static void physicalButtonTick(){
 const uint32_t now=millis();bool pressed=digitalRead(41)==LOW;
 if(pressed && !physicalButtonPrev){buttonStarted=now;buttonArmed=false;buttonRescueArmed=false;}
 if(pressed && !buttonArmed && (now-buttonStarted)>=3000UL){
  buttonArmed=true;physicalUntil=now+120000UL;
  record("FISICO_ARMADO","Boton GPIO41: ventana de autorizacion 120 s");
 }
 if(pressed && !buttonRescueArmed && (now-buttonStarted)>=8000UL){
  buttonRescueArmed=true;physicalUntil=now+300000UL;rescueUntil=now+300000UL;
  WiFi.softAPdisconnect(true);rescueOpen=WiFi.softAP("CIELO96-RESCATE");
  wifiApOn=rescueOpen;record("AP_RESCATE",rescueOpen?"AP temporal 5 min":"AP no disponible");
 }
 if(rescueOpen && int32_t(now-rescueUntil)>=0){
  WiFi.softAPdisconnect(true);rescueOpen=false;wifiApOn=false;
  record("AP_RESCATE_FIN","AP temporal cerrado");
 }
 physicalButtonPrev=pressed;
}
static void wifiTask(){
 const uint32_t now=millis();
 if(WiFi.status()==WL_CONNECTED){
  if(!mdnsStarted){mdnsStarted=MDNS.begin("iwagumi-cielo");if(mdnsStarted)MDNS.addService("http","tcp",80);}
  return;
 }
 if(storedSSID.length() && now-lastWiFiAttempt>30000UL){
  lastWiFiAttempt=now;WiFi.begin(storedSSID.c_str(),storedPass.c_str());wifiFailures++;
 }
 if(!wifiApOn && now-bootMs>20000){
  wifiApOn=WiFi.softAP("CIELO96-CONFIG",apPassword.c_str());
  record("AP_RECUPERACION",String("192.168.4.1; password en Serial. activo=")+String(wifiApOn));
 }
}
static void lightingTask(){
 uint32_t now=millis();
 if(safeBoot||!timeOK())return;
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
 }else if(!chiManual && (!within || !chiCieloNight) && !chiAutoAttempted){
  requestAuto();lastChDesired=false;
 }
}
static void bleTick(){
 if(safeBoot)return;
 uint32_t now=millis();cielo96::BleResult res{};
 while(cielo96::ble_result(res)){
  DevTelemetry& t=devices[res.device==1?1:0];t.completed++;t.write_calls+=res.write_calls;t.last_ms=res.duration_ms;
  latencyMs[res.device==1?1:0][latencyCount[res.device==1?1:0]++%32]=res.duration_ms;
  rtcBlePhase=0;
  if(res.duration_ms>t.max_ms)t.max_ms=res.duration_ms;
  if(res.duration_ms>3500)t.slow++;
  if(res.success)t.gatt_ok++;else t.errors++;
  queue.complete(res.sequence,res.success,now);bleResultCount++;
  if(res.duration_ms>bleDurationMax)bleDurationMax=res.duration_ms;
  if(res.duration_ms>3500UL){bleSlowCount++;record("BLE_LENTO",String(res.device?"CH ":"PV ")+String(res.duration_ms)+"ms");}
  if(!res.success){bleResultErrors++;lastBleError=String(res.device?"CH ":"PV ")+String(res.reason);record("BLE_ERROR",lastBleError);saveCriticalIncident(lastBleError);}
  else if(res.device==1 && chiAutoPending){chiAutoPending=false;record("CH_AUTO","GATT completado, efecto optico no verificado");}
 }
 queue.tick(now);
 const uint32_t timeoutNow=queue.metrics().soft_timeouts;
 if(timeoutNow>bleSoftTimeouts){bleSoftTimeouts=timeoutNow;record("BLE_SOFT_TIMEOUT","Web sigue activo; tarea BLE no liberada artificialmente");}
 cielo::ble::Command cmd{};
 if(queue.start_next(now,cmd)){
  rtcBlePhase=cmd.device==cielo::ble::Device::PezVerde?1:2;
  if(!cielo96::ble_dispatch(cmd)){rtcBlePhase=0;
    queue.complete(cmd.sequence,false,now);record("BLE_COLA_LLENA","No se envio comando al worker");
  }
 }
}
void setup(){
 bootMs=millis();Serial.begin(115200);delay(100);
 Serial.printf("IWAGUMI CIELO %s | ATOMS3-Lite | build experimental\n",VERSION);
 setenv("TZ","CET-1CEST,M3.5.0,M10.5.0/3",1);tzset();
 prefs.begin("cielo96",false);
 persistentBoots=prefs.getUInt("boot_count",0)+1;prefs.putUInt("boot_count",persistentBoots);
 persistedFault=prefs.getString("last_fault","");
 if(rtcCieloMagic==0xC1E10960UL)previousPhase=rtcBlePhase;
 else previousPhase=0;
 rtcCieloMagic=0xC1E10960UL;rtcBootCount=persistentBoots;rtcBlePhase=0;
 if(previousPhase)record("REINICIO_BLE_PREVIO",String("Ultima fase BLE=")+String(previousPhase)+"; causa no demostrada");
 loadProfile();storedSSID=prefs.getString("ssid",CIELO_WIFI_SSID);storedPass=prefs.getString("wifi_pw",CIELO_WIFI_PASSWORD);
 recoverOldWifi();
 // Emergency AP secret is created from hardware randomness on first boot,
 // persisted in NVS, printed ONLY on the physical serial port.
 apPassword=prefs.getString("ap_secret","");
 if(apPassword.length()<12){
   static const char alphabet[]="ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
   apPassword="";
   for(int i=0;i<14;i++)apPassword+=alphabet[esp_random()%(sizeof(alphabet)-1)];
   prefs.putString("ap_secret",apPassword);
 }
 pinMode(41,INPUT_PULLUP);safeBoot=(digitalRead(41)==LOW);
 WiFi.mode(WIFI_AP_STA);WiFi.setSleep(false);WiFi.setAutoReconnect(false);
 WiFi.setHostname("iwagumi-cielo96");
 if(storedSSID.length()){WiFi.begin(storedSSID.c_str(),storedPass.c_str());lastWiFiAttempt=millis();}
 configTime(0,0,"pool.ntp.org","time.google.com");
 bleReady=!safeBoot && cielo96::ble_begin();
 if(safeBoot)record("ARRANQUE_SEGURO","Boton fisico presionado: BLE deshabilitado");
 record("INICIO",String("reset=")+String((unsigned)esp_reset_reason())+" BLE="+String(bleReady));
 Serial.printf("WIFI de rescate si fuera necesaria: CIELO96-CONFIG | clave: %s\n",apPassword.c_str());
 configureRoutes();
}
void loop(){
 srv.handleClient();
 physicalButtonTick();
 wifiTask();
 lightingTask();
 bleTick();
 delay(3);
}
