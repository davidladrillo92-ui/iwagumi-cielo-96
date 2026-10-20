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
#include <esp_wifi.h>
#include <esp_attr.h>
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
#define CIELO_ENABLE_OTA 1 // OTA solo con clave local + concesion fisica 3-7s; RC, probar primero en USB
#endif

static constexpr const char* VERSION="10.0.3-RC4-NATURAL-V4-EN-VIVO";
static constexpr const char* CIELO_TZ="CET-1CEST,M3.5.0,M10.5.0/3";
// Ultima etapa conservada en RTC (indicio, no backtrace); no utiliza NVS/flash.
enum class TracePhase : uint32_t {
 Unknown=0, Setup=10, Nvs=20, Profile=30, WifiInit=40, Clock=50,
 WebInit=60, Http=70, Button=80, WifiTick=90, Ble=100, Led=110,
 Idle=120, RescueOpen=130, RescueClose=140
};
struct RtcTrace { uint32_t magic; uint32_t phase; uint32_t check; };
static constexpr uint32_t RTC_TRACE_MAGIC=0xC1E10964UL;
static constexpr uint32_t RTC_TRACE_SALT=0x6B3D821FUL;
RTC_NOINIT_ATTR static volatile RtcTrace rtcTrace;
static uint32_t lastResetTracePhase=0;
static bool lastResetTraceValid=false;
static const char* tracePhaseName(uint32_t p){
 switch(static_cast<TracePhase>(p)){
 case TracePhase::Setup:return "SETUP";
 case TracePhase::Nvs:return "NVS";
 case TracePhase::Profile:return "PERFIL";
 case TracePhase::WifiInit:return "WIFI_INICIO";
 case TracePhase::Clock:return "RELOJ_NTP";
 case TracePhase::WebInit:return "HTTP_INICIO";
 case TracePhase::Http:return "HTTP_ATENCION";
 case TracePhase::Button:return "BOTON";
 case TracePhase::WifiTick:return "WIFI_GESTION";
 case TracePhase::Ble:return "BLE";
 case TracePhase::Led:return "LED";
 case TracePhase::Idle:return "BUCLE_EN_ESPERA";
 case TracePhase::RescueOpen:return "RESCATE_ABRIENDO";
 case TracePhase::RescueClose:return "RESCATE_CERRANDO";
 default:return "NO_DISPONIBLE";
 }
}
static void tracePhase(TracePhase p){
 uint32_t v=static_cast<uint32_t>(p);
 rtcTrace.phase=v;
 rtcTrace.check=RTC_TRACE_MAGIC ^ RTC_TRACE_SALT ^ v;
 rtcTrace.magic=RTC_TRACE_MAGIC;
}

static WebServer srv(80);
static Preferences prefs;
static cielo96::ProfileImage profile;
static cielo::ble::Scheduler queue(3500,250,2000);
static bool bleReady=false, profileRecovery=false, wifiApOn=false;
static bool phoneSyncedThisBoot=false;
static bool wifiCoexistReady(){
 wifi_ps_type_t ps=WIFI_PS_NONE;
 return esp_wifi_get_ps(&ps)==ESP_OK && ps!=WIFI_PS_NONE;
}
static const char* wifiSleepName(){
 wifi_ps_type_t ps=WIFI_PS_NONE;
 if(esp_wifi_get_ps(&ps)!=ESP_OK)return "NO_DISPONIBLE";
 if(ps==WIFI_PS_NONE)return "NONE_INCOMPATIBLE_BLE";
 if(ps==WIFI_PS_MIN_MODEM)return "MIN_MODEM";
 if(ps==WIFI_PS_MAX_MODEM)return "MAX_MODEM";
 return "DESCONOCIDO";
}
static bool blePilotRequested=false, blePilotAttempted=false, rescueOpen=false;
// Observacion paralela en RAM: las pruebas pasivas no generan trafico GATT.
struct LabState {
 bool running=false;
 uint32_t started=0,lastSample=0,samples=0;
 int16_t rssiMin=127,rssiMax=-127;
 uint32_t heapMin=UINT32_MAX,heapLargestMin=UINT32_MAX;
 uint32_t baselineWifiDrops=0,baselineBleErrors=0,baselineHttp=0;
 uint32_t loopLast=0,loopMaxGapMs=0;
 uint32_t scanRequests=0,scanCompleted=0,scanRejects=0,scanTimeouts=0,scanStartedMs=0;
 bool scanPending=false;
 cielo96::BleScanResult lastScan{};
 uint32_t browserSamples=0,browserAvgMs=0,browserMaxMs=0;
 bool txArmed=false;
 uint32_t txUntil=0,txArmCount=0,txBlocked=0;
 uint32_t probeRequests=0,probeCompleted=0,probeRejected=0,probeTimeouts=0,probeStartedMs=0;
 bool probePending=false;
 cielo96::BleProbeResult probeResults[2]{};
 uint32_t dryRunChecks=0;
 uint32_t dryRunChecksum=0;
 uint8_t pvSimR=0,pvSimG=0,pvSimB=0,pvSimBrightness=0;
 uint8_t chSimR=0,chSimG=0,chSimB=0;
};
static LabState lab;
static constexpr uint32_t LAB_TX_WINDOW_MS=90000UL;
static bool customResumePending=false;
static uint32_t customResumeRequested=0,customResumeOk=0,customResumeFailed=0;
static String customResumeLast="Sin probar";
static uint32_t customResumeLastMs=0;
// Ensayo guiado: 0=reposo, 1=enviando 30, 2=GATT manual OK, 3=fallo manual,
// 4=enviando curva guardada, 5=GATT retorno OK, 6=fallo retorno, 7=caducado.
// El exito optico SOLO puede confirmarlo el propietario.
static uint8_t contrastStage=0;
static uint32_t contrastStartedAt=0,contrastManualAt=0;
static bool contrastObserved=false;
static const char* contrastStageName(){
 switch(contrastStage){
  case 1:return "enviando_30";case 2:return "esperando_confirmacion_visual";
  case 3:return "error_manual";case 4:return "restaurando_curva";
  case 5:return "retorno_gatt_ok_verificar_luz";case 6:return "error_retorno";
  case 7:return "ensayo_caducado";default:return "sin_prueba";
 }
}
static bool labTxEnabled(){
 return lab.txArmed && (int32_t)(lab.txUntil-millis())>0 &&
        bleReady && !rescueOpen && wifiCoexistReady();
}
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
static bool pezManual=false,chiManual=false,chiCieloNight=false,chiAutoPending=false;
// Modo ACTIVADO EXPLICITAMENTE. Tras reinicio exige pulsar otra vez.
static bool cieloPvRun=false,cieloChRun=false,cieloChWasDriven=false;
static bool cieloChReturnPending=false,cieloChAwaitResponse=false;
static bool cieloWatchdogTripped=false;
static uint8_t cieloChConsecutiveFailures=0,cieloPvConsecutiveFailures=0,cieloReturnTries=0;
static uint32_t cieloReturnRetryAt=0,cieloStartedAt=0;
static uint32_t cieloPvOrders=0,cieloChOrders=0,cieloPvConfirmed=0,cieloChConfirmed=0;
static uint32_t cieloReturnOk=0,cieloReturnFailed=0,cieloFallbacks=0;
static String cieloReturnReason="Sin solicitar";
static void cieloRequestReturn(const char* reason);
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
static char otaExpectedSha[65]{};
static mbedtls_sha256_context otaDigest;
static bool otaDigestStarted=false;
static uint8_t otaDigestOut[32]{};
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
// Los valores de latencia HTTP no son minutos: permitir hasta 60 segundos sin
// reinterpretar mediciones reales como fallos de ruta/entrada de usuario.
static bool parseBoundedUInt(const String&s,unsigned&n,unsigned maxValue){
 if(s.isEmpty()||s.length()>8)return false;
 uint32_t x=0;
 for(size_t i=0;i<s.length();i++){
  char ch=s[i];if(ch<'0'||ch>'9')return false;
  x=x*10+(ch-'0');if(x>maxValue)return false;
 }
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
static String cieloModeJson(){
 String j="{\"pez_verde_active\":"+String(cieloPvRun?"true":"false");
 j+=",\"chihiros_active\":"+String(cieloChRun?"true":"false");
 j+=",\"chihiros_owns_light\":"+String(cieloChWasDriven?"true":"false");
 j+=",\"return_pending\":"+String(cieloChReturnPending||cieloChAwaitResponse?"true":"false");
 j+=",\"return_reason\":"+jsonQuote(cieloReturnReason);
 j+=",\"pv_sent\":"+String(cieloPvOrders)+",\"pv_gatt_ok\":"+String(cieloPvConfirmed);
 j+=",\"ch_sent\":"+String(cieloChOrders)+",\"ch_gatt_ok\":"+String(cieloChConfirmed);
 j+=",\"return_ok\":"+String(cieloReturnOk)+",\"return_failed\":"+String(cieloReturnFailed);
 j+=",\"safety_fallbacks\":"+String(cieloFallbacks);
 j+=",\"pv_consecutive_errors\":"+String(cieloPvConsecutiveFailures);
 j+=",\"ch_consecutive_errors\":"+String(cieloChConsecutiveFailures);
 j+=",\"started_at_ms\":"+String(cieloStartedAt);
 j+=",\"note\":\"La recuperacion 0x03 requiere BLE operativo y energia del ATOM\"}";
 return j;
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
 j+=",\"cieloV4\":"+cieloModeJson();
 j+=",\"chActive\":"+String(cieloChRun?"true":"false")+",\"chEvening\":"+String(cieloChRun?"true":"false")+",\"chContinuous\":"+String(cieloChRun?"true":"false");
 j+=",\"chStatus\":"+jsonQuote(cieloChRun?"Natural V4: control por CIELO":cieloChReturnPending||cieloChAwaitResponse?"Restaurando curva de My Chihiros":"My Chihiros: curva personalizada");
 j+=",\"chEveningStatus\":"+jsonQuote(cieloChRun?"NATURAL_V4_ACTIVA":"MY_CHIHIROS")+",\"bleFailures\":"+String(bleResultErrors);
 j+=",\"txFrames\":"+String(devices[0].write_calls)+",\"txBatches\":"+String(devices[0].completed);
 j+=",\"bleError\":"+jsonQuote(lastBleError)+",\"bleLive\":false,\"storm\":false,\"stormStatus\":\"No disponible en R4 DEV\"";
 j+=",\"chFrames\":"+String(devices[1].write_calls)+",\"chFailures\":"+String(devices[1].errors);
 j+=",\"chLiveRgb\":["+String(c.r)+","+String(c.g)+","+String(c.b)+"],\"chLiveFailures\":0,\"chAutoAttempts\":"+String(chiAutoAttempted?1:0);
 j+=",\"otaPaired\":"+String(prefs.getBytesLength("ota_sha")==32?"true":"false")+"}";
 jsonSend(200,j);
}
static bool physicalOtaGrant();
static String labJson(){
 const bool connected=WiFi.status()==WL_CONNECTED;
 const bool clockOK=time(nullptr)>1700000000L;
 const bool profOK=cielo96::profileValid(profile);
 const uint32_t age=millis();
 String j;j.reserve(2600);
 j="{\"schema\":\"cielo-lab-v1\",\"running\":"+String(lab.running?"true":"false");
 j+=",\"session_elapsed_sec\":"+String(lab.running?(age-lab.started)/1000UL:0);
 j+=",\"samples\":"+String(lab.samples)+",\"sample_interval_ms\":15000";
 j+=",\"checks\":{\"wifi\":"+String(connected?"true":"false");
 j+=",\"coexistence\":"+String(wifiCoexistReady()?"true":"false");
 j+=",\"clock\":"+String(clockOK?"true":"false");
 j+=",\"profile_crc\":"+String(profOK?"true":"false");
 j+=",\"ap\":"+String(wifiApOn?"true":"false");
 j+=",\"ble_worker\":"+String(bleReady?"true":"false")+"}";
 j+=",\"rssi_min\":"+String(lab.samples?lab.rssiMin:0)+",\"rssi_max\":"+String(lab.samples?lab.rssiMax:0);
 j+=",\"heap_min\":"+String(lab.samples?lab.heapMin:0);
 j+=",\"heap_largest_block_min\":"+String(lab.samples?lab.heapLargestMin:0);
 j+=",\"loop_max_gap_ms\":"+String(lab.loopMaxGapMs);
 j+=",\"wifi_drop_delta\":"+String(wifiDisconnects-lab.baselineWifiDrops);
 j+=",\"ble_error_delta\":"+String(bleResultErrors-lab.baselineBleErrors);
 j+=",\"http_requests_delta\":"+String(httpCount-lab.baselineHttp);
 j+=",\"browser_rtt\":{\"samples\":"+String(lab.browserSamples);
 j+=",\"avg_ms\":"+String(lab.browserAvgMs)+",\"max_ms\":"+String(lab.browserMaxMs)+"}";
 j+=",\"discovery_scan\":{\"requested\":"+String(lab.scanRequests);
 j+=",\"completed\":"+String(lab.scanCompleted)+",\"rejected\":"+String(lab.scanRejects)+",\"timeouts\":"+String(lab.scanTimeouts);
 j+=",\"pending\":"+String(lab.scanPending?"true":"false");
 j+=",\"last_scan_duration_ms\":"+String(lab.lastScan.elapsed_ms);
 j+=",\"found_pez_verde\":"+String(lab.lastScan.pez_found?"true":"false");
 j+=",\"pez_rssi\":"+String(lab.lastScan.pez_rssi);
 j+=",\"found_chihiros\":"+String(lab.lastScan.chihiros_found?"true":"false");
 j+=",\"chihiros_rssi\":"+String(lab.lastScan.chihiros_rssi);
 j+=",\"advertisements\":"+String(lab.lastScan.advertised);
 j+=",\"named_advertisements\":"+String(lab.lastScan.named);
 j+=",\"anonymous_advertisements\":"+String(lab.lastScan.unnamed);
 j+=",\"active_scan\":true,\"scan_duration_requested_sec\":5";
 j+=",\"nearby\":[";
 for(uint8_t i=0;i<lab.lastScan.observed_count;i++){
  const auto& entry=lab.lastScan.observed[i];
  if(i)j+=",";
  j+="{\"name\":"+jsonQuote(String(entry.name));
  j+=",\"rssi\":"+String(entry.rssi);
  j+=",\"nordic_uart\":"+String(entry.nus?"true":"false")+"}";
 }
 j+="]}";
 j+=",\"gatt_probe\":{\"requested\":"+String(lab.probeRequests);
 j+=",\"completed\":"+String(lab.probeCompleted)+",\"rejected\":"+String(lab.probeRejected);
 j+=",\"timeouts\":"+String(lab.probeTimeouts)+",\"pending\":"+String(lab.probePending?"true":"false");
 j+=",\"devices\":[";
 for(uint8_t k=0;k<2;k++){
  if(k)j+=",";
  const auto& probe=lab.probeResults[k];
  j+="{\"device\":"+jsonQuote(k?"chihiros":"pez_verde");
  j+=",\"success\":"+String(probe.success?"true":"false");
  j+=",\"service\":"+String(probe.service?"true":"false");
  j+=",\"characteristic\":"+String(probe.characteristic?"true":"false");
  j+=",\"duration_ms\":"+String(probe.elapsed_ms);
  j+=",\"reason\":"+jsonQuote(String(probe.reason))+"}";
 }
 j+="]}";
 j+=",\"links\":{\"pez_verde\":{";
 for(uint8_t linkIndex=0;linkIndex<2;linkIndex++){
  const auto link=cielo96::ble_link_status(linkIndex);
  if(linkIndex)j+=",\"chihiros\":{";
  j+="\"connected\":"+String(link.connected?"true":"false");
  j+=",\"released_to_app\":"+String(link.released_to_app?"true":"false");
  j+=",\"release_count\":"+String(link.release_count);
  j+=",\"last_disconnect_ms\":"+String(link.last_disconnect_ms)+"}";
 }
 j+="}";
 j+=",\"chihiros_contrast\":{\"stage\":"+jsonQuote(contrastStageName());
 j+=",\"expected_before\":[70,65,60],\"test_rgb\":[30,30,30]";
 j+=",\"elapsed_sec\":"+String(contrastStartedAt?(age-contrastStartedAt)/1000UL:0);
 j+=",\"manual_gatt_completed\":"+String(contrastStage>=2&&contrastStage!=3?"true":"false");
 j+=",\"contrast_observed_by_user\":"+String(contrastObserved?"true":"false");
 j+=",\"optical_restoration_confirmed\":false}";
 j+=",\"chihiros_my_curve\":{\"experimental\":true,\"confirmed_optically\":false";
 j+=",\"pending\":"+String(customResumePending?"true":"false");
 j+=",\"requests\":"+String(customResumeOk+customResumeFailed+(customResumePending?1:0));
 j+=",\"gatt_ok\":"+String(customResumeOk)+",\"gatt_failed\":"+String(customResumeFailed);
 j+=",\"last_result\":"+jsonQuote(customResumeLast)+"}";
 j+=",\"lighting\":{\"scheduler_enabled\":"+String(cieloPvRun||cieloChRun?"true":"false")+",\"gatt_tx_armed\":"+String(labTxEnabled()?"true":"false");
 j+=",\"tx_window_remaining_sec\":"+String(labTxEnabled()?(lab.txUntil-age)/1000UL:0);
 j+=",\"tx_arm_count\":"+String(lab.txArmCount)+",\"tx_blocked\":"+String(lab.txBlocked);
 j+=",\"dry_run_checks\":"+String(lab.dryRunChecks);
 j+=",\"dry_run_checksum\":"+String(lab.dryRunChecksum);
 j+=",\"dry_run_pez_rgb\":["+String(lab.pvSimR)+","+String(lab.pvSimG)+","+String(lab.pvSimB)+"]";
 j+=",\"dry_run_pez_brightness\":"+String(lab.pvSimBrightness);
 j+=",\"dry_run_chihiros_rgb\":["+String(lab.chSimR)+","+String(lab.chSimG)+","+String(lab.chSimB)+"]";
 j+=",\"pv_tx_frames\":"+String(devices[0].write_calls);
 j+=",\"ch_tx_frames\":"+String(devices[1].write_calls);
 j+=",\"chihiros_auto_verified\":false,\"auto_command_disabled\":true}}";
 return j;
}
static void labTick(){
 const uint32_t now=millis();
 cielo96::BleCustomResumeResult custom{};
 while(cielo96::ble_custom_resume_result(custom)){
  customResumePending=false;customResumeLastMs=now;
  customResumeLast=String(custom.reason)+" "+String(custom.duration_ms)+" ms; verificacion visual pendiente";
  if(custom.success){
   customResumeOk++;if(contrastStage==4)contrastStage=5;
   record("CH_CURVA_MYCH_GATT_OK",customResumeLast);
  }else{
   customResumeFailed++;if(contrastStage==4)contrastStage=6;
   record("CH_CURVA_MYCH_FALLO",customResumeLast);
  }
 }
 if(customResumePending && now-customResumeRequested>24000UL){
  customResumePending=false;customResumeFailed++;
  customResumeLast="Sin respuesta de la tarea BLE en 24 s; no repetir automaticamente";
  if(contrastStage==4)contrastStage=6;
  record("CH_CURVA_MYCH_TIMEOUT",customResumeLast);
 }
 if(contrastStage==1 && now-contrastStartedAt>30000UL){
  contrastStage=3;
  lab.txArmed=false;
  record("CH_CONTRASTE_TIMEOUT","30/30/30 no confirmado por tarea BLE; control manual desde My Chihiros");
 }
 if(contrastStage==2 && now-contrastManualAt>75000UL){
  contrastStage=7;lab.txArmed=false;
  cielo96::ble_release_request(1);
  record("CH_CONTRASTE_CADUCADO","No se solicito retorno a curva en 75s; recuperar desde My Chihiros");
 }
 if(lab.loopLast && now-lab.loopLast>lab.loopMaxGapMs)lab.loopMaxGapMs=now-lab.loopLast;
 lab.loopLast=now;
 cielo96::BleProbeResult probe{};
 while(cielo96::ble_probe_result(probe)){
  lab.probeResults[probe.device==1?1:0]=probe;
  lab.probeCompleted++;lab.probePending=false;
  record(probe.success?"LAB_GATT_PROBE_OK":"LAB_GATT_PROBE_FALLO",
   String(probe.device?"CH ":"PV ")+String(probe.reason)+" "+String(probe.elapsed_ms)+"ms sin escrituras");
 }
 if(lab.probePending && now-lab.probeStartedMs>20000UL){
  lab.probePending=false;lab.probeTimeouts++;
  record("LAB_GATT_PROBE_TIMEOUT","Enlace no contestó en 20s; no iniciar otra sonda");
 }
 cielo96::BleScanResult result{};
 while(cielo96::ble_scan_result(result)){
  lab.lastScan=result;lab.scanCompleted++;lab.scanPending=false;
  record("LAB_SCAN_RESULT",String("PV=")+(result.pez_found?"1":"0")+" CH="+(result.chihiros_found?"1":"0")+" anuncios="+String(result.advertised)+" con_nombre="+String(result.named)+" anonimos="+String(result.unnamed));
 }
 if(lab.scanPending && now-lab.scanStartedMs>16000UL){
  lab.scanPending=false;lab.scanTimeouts++;
  record("LAB_SCAN_TIMEOUT","Escaneo sin respuesta tras 16 segundos");
 }
 if(lab.txArmed && (int32_t)(now-lab.txUntil)>=0){
  lab.txArmed=false;
  record("LAB_TX_CADUCADA","Envios bloqueados; restauracion optica NO garantizada");
 }
 if(pezManual && (int32_t)(now-pezManualUntil)>=0)pezManual=false;
 if(chiManual && (int32_t)(now-chiManualUntil)>=0)chiManual=false;
 if(!lab.running || now-lab.lastSample<15000UL)return;
 lab.lastSample=now;lab.samples++;
 if(WiFi.status()==WL_CONNECTED){
  int16_t rssi=WiFi.RSSI();
  if(rssi<lab.rssiMin)lab.rssiMin=rssi;
  if(rssi>lab.rssiMax)lab.rssiMax=rssi;
 }
 const uint32_t heap=heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
 const uint32_t block=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
 if(heap<lab.heapMin)lab.heapMin=heap;
 if(block<lab.heapLargestMin)lab.heapLargestMin=block;
 // Evaluacion real de ambas curvas en RAM; los resultados se guardan como
 // prueba verificable, pero NUNCA se trasladan a la cola GATT.
 if(timeOK()){
  const auto pv=cielo::at(profile.pv,profile.pvCount,minuteOfDay());
  const auto ch=cielo::at(profile.ch,profile.chCount,minuteOfDay());
  lab.pvSimR=pv.r;lab.pvSimG=pv.g;lab.pvSimB=pv.b;lab.pvSimBrightness=pv.brightness;
  lab.chSimR=ch.r;lab.chSimG=ch.g;lab.chSimB=ch.b;
  lab.dryRunChecksum^=(uint32_t(pv.r)<<24)^(uint32_t(pv.brightness)<<16)^(uint32_t(ch.r)<<8)^uint32_t(ch.b);
  lab.dryRunChecks++;
 }
}
static void labStart(){
 lab=LabState{};
 lab.running=true;lab.started=millis();lab.lastSample=millis()-15000UL;
 lab.baselineWifiDrops=wifiDisconnects;lab.baselineBleErrors=bleResultErrors;lab.baselineHttp=httpCount;
 record("LAB_INICIO","Muestreo simultaneo WiFi RAM reloj curvas HTTP; sin GATT");
}
static void diagnosticJson(){
 const auto&m=queue.metrics();bool wifi=WiFi.status()==WL_CONNECTED;
 uint32_t freeMem=heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
 uint32_t minimum=heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
 uint32_t largest=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
 bool badReset=abnormalReset(esp_reset_reason());
 bool workerUnavailable=bleReady && (millis()-cielo96::ble_last_heartbeat()>45000UL);
 bool warning=badReset||(!bleReady&&blePilotAttempted)||(!wifi&&!wifiApOn)||workerUnavailable||bleResultErrors||bleSlowCount||profileRecovery||m.soft_timeouts||otaError||!timeOK();
 String j;j.reserve(11500);
 j="{\"schema\":\"iwagumi-cielo-96-diagnostico-v2\",\"firmware\":"+jsonQuote(VERSION)+",\"generated_at\":"+jsonQuote(dateTime());
 j+=",\"uptime_sec\":"+String((millis()-bootMs)/1000UL)+",\"status\":"+jsonQuote(warning?"avisos_detectados":"sin_avisos_actuales");
 j+=",\"pilot\":{\"ble_start_requires_button\":false,\"ble_start_attempted\":"+String(blePilotAttempted?"true":"false")+",\"rescue_read_only\":"+String(rescueOpen?"true":"false")+",\"rescue_seconds\":"+String(rescueOpen?(rescueUntil-millis())/1000UL:0)+",\"private_ap_active\":"+String(apProtected?"true":"false")+"}";
 j+=",\"system\":{\"reset_reason\":"+String((unsigned)esp_reset_reason())+",\"reset_reason_name\":"+jsonQuote(resetReasonName(esp_reset_reason()))+",\"abnormal_reset\":"+String(badReset?"true":"false")+",\"boot_marked_healthy\":"+String(currentBootMarkedHealthy?"true":"false")+",\"watchdog_active\":null";
 j+=",\"heap_free\":"+String(freeMem)+",\"heap_minimum\":"+String(minimum)+",\"largest_heap_block\":"+String(largest);
 j+=",\"heap_metric\":\"MALLOC_CAP_INTERNAL\",\"filesystem_ok\":null,\"filesystem_errors\":null";
 j+=",\"boot_count\":"+String(previousBootCount+1)+",\"previous_boot_marked_clean\":"+String(priorBootClean?"true":"false");
 j+=",\"last_phase\":"+jsonQuote(tracePhaseName(rtcTrace.phase));
 j+=",\"previous_reset_phase_valid\":"+String(lastResetTraceValid?"true":"false");
 j+=",\"previous_reset_phase\":"+jsonQuote(lastResetTraceValid?tracePhaseName(lastResetTracePhase):"NO_DISPONIBLE");
 j+=",\"previous_reset_phase_is_hint_only\":true";
 j+=",\"persistent_fault_count\":"+String(savedFailures)+",\"prior_fault_type\":"+jsonQuote(priorEvent)+",\"prior_fault_detail\":"+jsonQuote(priorEventDetail)+"}";
 j+=",\"network\":{\"home_wifi_connected\":"+String(wifi?"true":"false")+",\"ap_enabled\":"+String(wifiApOn?"true":"false");
 j+=",\"wifi_disconnects\":"+String(wifiDisconnects)+",\"wifi_connection_attempts\":"+String(wifiFailures)+",\"wifi_rssi_dbm\":"+String(wifi?WiFi.RSSI():0);
 j+=",\"wifi_modem_sleep\":"+jsonQuote(wifiSleepName())+",\"wifi_ble_coex_ready\":"+String(wifiCoexistReady()?"true":"false")+"}";
 j+=",\"lighting\":{\"natural_v4\":"+cieloModeJson();
 j+=",\"pez_verde\":{\"mode\":"+jsonQuote(cieloPvRun?"NATURAL_V4_CIELO":pezManual?"manual":"programa")+",\"tx_batches\":"+String(devices[0].completed);
 j+=",\"tx_frames\":"+String(devices[0].write_calls)+",\"errors\":"+String(devices[0].errors)+"},\"chihiros\":{\"cielo_night_enabled\":"+String(cieloChRun?"true":"false");
 j+=",\"ble_active\":"+String(bleReady?"true":"false")+",\"auto_restore_pending\":"+String(chiAutoPending?"true":"false")+",\"tx_frames\":"+String(devices[1].write_calls)+",\"errors\":"+String(devices[1].errors)+"}}";
 j+=",\"bluetooth\":{\"worker_heartbeat_age_ms\":"+String(millis()-cielo96::ble_last_heartbeat());
 for(uint8_t linkIndex=0;linkIndex<2;linkIndex++){
  const auto link=cielo96::ble_link_status(linkIndex);
  j+=",\"link_";
  j+=linkIndex?"chihiros":"pez_verde";
  j+="\":{\"connected\":"+String(link.connected?"true":"false");
  j+=",\"released_to_app\":"+String(link.released_to_app?"true":"false");
  j+=",\"release_count\":"+String(link.release_count)+"}";
 }
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
 j+=",\"physical_ota_grant\":"+String(physicalOtaGrant()?"true":"false");
 j+=",\"ota_enabled\":"+String(CIELO_ENABLE_OTA?"true":"false")+"}";
 time_t currentEpoch=time(nullptr);
 struct tm clockLocal{};
 char clockOffset[8]="";
 if(timeOK()){
  localtime_r(&currentEpoch,&clockLocal);
  strftime(clockOffset,sizeof(clockOffset),"%z",&clockLocal);
 }
 j+=",\"clock_diagnostics\":{\"epoch\":"+String((uint32_t)currentEpoch);
 j+=",\"valid\":"+String(timeOK()?"true":"false");
 j+=",\"timezone\":\"Europe/Madrid\",\"utc_offset\":"+jsonQuote(clockOffset);
 j+=",\"daylight_saving\":"+String(timeOK()&&clockLocal.tm_isdst>0?"true":"false");
 j+=",\"phone_sync_this_boot\":"+String(phoneSyncedThisBoot?"true":"false")+"}";
 j+=",\"lab\":"+labJson();
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
 if(!labTxEnabled()){lab.txBlocked++;record("LAB_TX_BLOQUEADA","Pez Verde: falta autorizacion");return;}
 if(!queue.submit(cielo::ble::Device::PezVerde,cielo::ble::Action::Apply,p.r,p.g,p.b,p.brightness,millis(),ttl))record("COLA_RECHAZADA","PV");
}
static void submitCh(cielo::ChiValue c,uint32_t ttl=90000){
 if(!labTxEnabled()){lab.txBlocked++;record("LAB_TX_BLOQUEADA","Chihiros: falta autorizacion");return;}
 if(!queue.submit(cielo::ble::Device::Chihiros,cielo::ble::Action::Apply,c.r,c.g,c.b,0,millis(),ttl))record("COLA_RECHAZADA","CH");
}
static void requestAuto(){
 // No conocemos si la Chihiros usa curva personalizada o escena.
 // 0x12 se deshabilita: apago fisicamente la WRGB Slim en la prueba 22.
 chiAutoPending=false;
 lab.txBlocked++;
 record("CH_AUTO_INSEGURO","Restauracion BLE bloqueada hasta verificar modo desde My Chihiros");
}
static void manualPv(){
 if(!labTxEnabled() || srv.arg("lab_manual")!="1")return fail(423,"Laboratorio: habilita 90 segundos de transmisiones manuales");
 if(srv.arg("mode")=="auto"){pezManual=false;lastPvDesired=false;submitPv(desiredPv());return ok();}
 if(!timeOK())return fail(503,"Reloj aun sin sincronizar");
 unsigned a[4]{};const char* k[]={"r","g","b","bright"};
 for(int i=0;i<4;i++){if(!positiveInt(srv.arg(k[i]),a[i])||a[i]>(i==3?100:255))return fail(400,"RGB o brillo incorrecto");}
 pezManual=true;pezManualUntil=millis()+20000UL;
 submitPv({uint8_t(a[0]),uint8_t(a[1]),uint8_t(a[2]),uint8_t(a[3])},20000);record("PV_MANUAL","20 segundos");ok();
}
static void manualCh(){
 if(!labTxEnabled() || srv.arg("lab_manual")!="1")return fail(423,"Laboratorio: habilita 90 segundos de transmisiones manuales");
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
  tracePhase(TracePhase::RescueOpen);
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
  if(heldMs>=3000UL && heldMs<8000UL && !rescueOpen){
   if(!blePilotAttempted){
    blePilotRequested=true;record("BLE_PILOTO_SOLICITADO","BLE activado manualmente");
   }else if(WiFi.status()==WL_CONNECTED){
    otaPhysicalUntil=now+90000UL;
    record("OTA_VENTANA_FISICA","90 segundos para carga privada desde Bambu; requiere clave");
   }
  }
 }
 if(rescueOpen && (int32_t)(now-rescueUntil)>=0){
  tracePhase(TracePhase::RescueClose);
  WiFi.softAPdisconnect(true);
  wifiApOn=WiFi.softAP("CIELO96-CONFIG",apPassword.c_str());
  apProtected=wifiApOn;rescueOpen=false;
  record("RESCATE_CERRADO","AP privado restaurado despues de 120s");
 }
}
static bool physicalOtaGrant(){
 // Mantiene OTA inaccesible en AP abierta de rescate y sin Bambu.
 return CIELO_ENABLE_OTA && !rescueOpen && WiFi.status()==WL_CONNECTED &&
        otaPhysicalUntil && (int32_t)(otaPhysicalUntil-millis())>0;
}
static void otaPair(){
 if(!CIELO_ENABLE_OTA)return fail(503,"OTA bloqueada en version experimental");
 if(prefs.getBytesLength("ota_sha")==32)return fail(409,"La clave OTA ya esta configurada");
 if(!physicalOtaGrant())return fail(403,"Mantener pulsado el boton fisico GPIO41 durante 5 segundos antes del emparejamiento");
 String s=srv.arg("key");if(s.length()<12||s.length()>96)return fail(400,"Clave OTA de 12 a 96 caracteres");
 uint8_t digest[32];if(!sha256String(s,digest)||prefs.putBytes("ota_sha",digest,32)!=32)return fail(507,"No se pudo guardar la clave");
 record("OTA_EMPAREJADA","Clave hasheada SHA256. No se registra en texto");ok();
}
static bool hashLooksValid(const String& h){
 if(h.length()!=64)return false;
 for(size_t i=0;i<h.length();i++)if(!isxdigit(static_cast<unsigned char>(h[i])))return false;
 return true;
}
static void otaDigestStop(){
 if(otaDigestStarted){mbedtls_sha256_free(&otaDigest);otaDigestStarted=false;}
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
  otaAllowed=tokenEqual() && physicalOtaGrant() && currentBootMarkedHealthy &&
             !labTxEnabled() && !queue.busy() && !otaBusy;
  otaSuccess=false;otaError=false;otaBytes=0;otaDigestStop();
  if(!otaAllowed){lastOtaError="Requiere clave y permiso de 90s del boton ATOM";otaError=true;return;}
  const String expected=srv.header("X-Cielo-Sha256");
  if(!hashLooksValid(expected) || !upload.filename.endsWith(".bin")){
   lastOtaError="BIN o SHA256 del navegador no valido";otaError=true;return;
  }
  strncpy(otaExpectedSha,expected.c_str(),64);otaExpectedSha[64]='\0';
  const esp_partition_t* target=esp_ota_get_next_update_partition(nullptr);
  if(!target || target->size<1048576 || target->size>0x330000 ||
     !Update.begin(target->size,U_FLASH)){
   lastOtaError="Particion OTA no compatible";otaError=true;return;
  }
  mbedtls_sha256_init(&otaDigest);
  if(mbedtls_sha256_starts_ret(&otaDigest,0)!=0){
   lastOtaError="No se pudo iniciar SHA256";otaError=true;Update.abort();mbedtls_sha256_free(&otaDigest);return;
  }
  otaDigestStarted=true;
  otaBusy=true;record("OTA_START","BIN manual, SHA256 y autorizacion fisica");
 }else if(upload.status==UPLOAD_FILE_WRITE){
  if(!otaAllowed||otaError)return;
  const esp_partition_t* target=esp_ota_get_next_update_partition(nullptr);
  if(!target || otaBytes+upload.currentSize>target->size ||
    mbedtls_sha256_update_ret(&otaDigest,upload.buf,upload.currentSize)!=0 ||
    Update.write(upload.buf,upload.currentSize)!=upload.currentSize){
   lastOtaError="Escritura OTA interrumpida o supera la particion";otaError=true;Update.abort();otaDigestStop();
  }else otaBytes+=upload.currentSize;
 }else if(upload.status==UPLOAD_FILE_END){
  if(!otaAllowed||otaError){otaBusy=false;otaDigestStop();return;}
  if(!otaDigestStarted || mbedtls_sha256_finish_ret(&otaDigest,otaDigestOut)!=0){
   otaError=true;lastOtaError="SHA256 no disponible";Update.abort();otaDigestStop();otaBusy=false;return;
  }
  otaDigestStop();
  static const char hex[]="0123456789abcdef";
  char actual[65];for(unsigned i=0;i<32;i++){actual[i*2]=hex[otaDigestOut[i]>>4];actual[i*2+1]=hex[otaDigestOut[i]&15];}
  actual[64]='\0';
  if(strcasecmp(actual,otaExpectedSha)!=0 || otaBytes<500000){
   otaError=true;lastOtaError="Hash o tamano de firmware no coinciden";Update.abort();
  }else if(!Update.end(true)){
   lastOtaError="BIN no validado como firmware ESP32-S3";otaError=true;Update.abort();
  }else{
   // Pista para regreso automatico ante reinicios tempranos repetidos. No
   // garantiza rescate de una imagen que ni siquiera llegue a ejecutar setup().
   prefs.putString("ota_prev",otaRunningPartition);
   prefs.putUChar("ota_attempts",0);
   otaSuccess=true;lastOtaError="SHA256 coincide y Update acepta el BIN";
   otaPhysicalUntil=0;
   record("OTA_OK",String("bytes=")+String(otaBytes)+" validado");
  }
  otaBusy=false;
 }else if(upload.status==UPLOAD_FILE_ABORTED){
  otaError=true;otaBusy=false;lastOtaError="OTA abortada";Update.abort();otaDigestStop();
 }
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
 const char*keys[]={"X-Cielo-Key","X-Cielo-Sha256"};srv.collectHeaders(keys,2);
 srv.on("/",HTTP_GET,[]{++httpCount;srv.sendHeader("Cache-Control","no-store");srv.send_P(200,"text/html; charset=utf-8",CIELO_UI);});
 srv.on("/api/status",HTTP_GET,statusJson);
 srv.on("/api/diagnostico/maestro",HTTP_GET,[]{if(srv.hasArg("download"))srv.sendHeader("Content-Disposition","attachment; filename=\"CIELO_INFORME_MAESTRO.json\"");diagnosticJson();});
 srv.on("/api/service/report",HTTP_GET,diagnosticJson);
 srv.on("/api/lab/status",HTTP_GET,[]{jsonSend(200,labJson());});
 srv.on("/api/lab/start",HTTP_POST,[]{
  if(rescueOpen)return fail(403,"Rescate es solo lectura");
  if(lab.scanPending)return fail(409,"Termina primero el escaneo BLE");
  labStart();ok();
 });
 srv.on("/api/lab/stop",HTTP_POST,[]{
  if(rescueOpen)return fail(403,"Rescate es solo lectura");
  lab.running=false;lab.txArmed=false;record("LAB_PARADA","Muestreo y transmision manual detenidos");ok();
 });
 srv.on("/api/lab/scan",HTTP_POST,[]{
  if(rescueOpen)return fail(403,"Rescate es solo lectura");
  if(!bleReady)return fail(423,"Inicia Bluetooth con boton fisico 3-7 segundos, sin enviar ordenes");
  if(lab.scanPending||lab.probePending)return fail(409,"Ya hay otra prueba BLE en marcha");
  if(!cielo96::ble_scan_request()){lab.scanRejects++;return fail(503,"Cola de escaneos no disponible");}
  lab.scanRequests++;lab.scanPending=true;lab.scanStartedMs=millis();record("LAB_SCAN_START","Escaneo BLE activo 5s; sin conectar ni escribir GATT");ok();
 });
 srv.on("/api/lab/probe",HTTP_POST,[]{
  if(rescueOpen)return fail(403,"Rescate es solo lectura");
  if(!bleReady)return fail(423,"Bluetooth todavia no inicializado");
  if(lab.scanPending||lab.probePending||queue.busy()||labTxEnabled())
   return fail(409,"Deten los envios y espera a que terminen las otras operaciones");
  if(srv.arg("confirm")!="SOLO_CONEXION_SIN_ESCRIBIR")return fail(403,"Confirmacion necesaria");
  const String device=srv.arg("device");
  if(device!="chihiros"&&device!="pez_verde")return fail(400,"Dispositivo desconocido");
  uint8_t index=device=="chihiros"?1:0;
  if(!cielo96::ble_probe_request(index)){lab.probeRejected++;return fail(503,"Cola de GATT ocupada");}
  lab.probePending=true;lab.probeStartedMs=millis();lab.probeRequests++;
  record("LAB_GATT_PROBE_INICIO",device+" sin escrituras; desconexion al terminar");ok();
 });
 srv.on("/api/chihiros/contrast/start",HTTP_POST,[]{
  if(rescueOpen)return fail(403,"Rescate solo lectura");
  if(srv.arg("confirm")!="ENSAYO_30_30_30")return fail(403,"Confirmacion necesaria");
  if(!bleReady||!wifiCoexistReady()||!timeOK())return fail(423,"Bluetooth o reloj sin preparar");
  if(queue.busy()||customResumePending||lab.scanPending||lab.probePending||
     contrastStage==1||contrastStage==4)return fail(409,"Espera a que termine el enlace anterior");
  lab.txArmed=true;lab.txUntil=millis()+LAB_TX_WINDOW_MS;lab.txArmCount++;
  if(!queue.submit(cielo::ble::Device::Chihiros,cielo::ble::Action::Apply,
                   30,30,30,0,millis(),45000)){
   lab.txArmed=false;return fail(503,"No se pudo enviar el contraste 30");
  }
  // La programacion diaria de CIELO se desactiva durante este ensayo: no
  // se debe superponer a la curva que administra My Chihiros.
  chiCieloNight=false;chiManual=true;chiManualUntil=millis()+120000UL;
  contrastStage=1;contrastObserved=false;contrastStartedAt=millis();contrastManualAt=0;
  record("CH_CONTRASTE_INICIO","Se solicita 30/30/30 y se espera confirmacion visual del usuario");
  ok();
 });
 srv.on("/api/chihiros/contrast/cancel",HTTP_POST,[]{
  if(rescueOpen)return fail(403,"Rescate solo lectura");
  if(queue.busy()||contrastStage==1||contrastStage==4)
   return fail(409,"Espera a que termine el envío Bluetooth antes de cancelar");
  lab.txArmed=false;chiManual=false;
  if(contrastStage==1||contrastStage==2)contrastStage=7;
  if(!queue.busy())cielo96::ble_release_request(1);
  record("CH_CONTRASTE_ABORTADO","Ensayo cancelado: recuperar desde My Chihiros si es necesario");
  ok();
 });
 srv.on("/api/chihiros/my-curve-test",HTTP_POST,[]{
  if(rescueOpen)return fail(403,"Rescate solo lectura");
  if(!bleReady || !labTxEnabled())return fail(423,"Bluetooth no preparado o ensayo sin autorizacion de 90 segundos");
  if(srv.arg("confirm")!="PROBAR_CURVA_MY_CHIHIROS")return fail(403,"Se requiere confirmacion explicita");
  if(contrastStage!=2)return fail(409,"Primero envia 30/30/30 y confirma que la operacion GATT termine");
  if(customResumePending||lab.scanPending||lab.probePending||queue.busy())
   return fail(409,"Bluetooth ocupado: espera a terminar");
  if(!cielo96::ble_custom_resume_request())return fail(503,"No se pudo programar ensayo seguro");
  customResumePending=true;customResumeRequested=millis();
  contrastStage=4;contrastObserved=true;
  chiManual=false;chiAutoPending=false;
  record("CH_CURVA_MYCH_ENSAYO","0x05 [03 FF FF] seleccion curva personalizada; no modifica los puntos; NO probado opticamente");
  ok();
 });
 srv.on("/api/ble/release",HTTP_POST,[]{
  if(rescueOpen)return fail(403,"Rescate solo lectura");
  if(!bleReady)return fail(423,"Bluetooth aun no inicializado");
  if(queue.busy()||lab.probePending||lab.scanPending)return fail(409,"Espera a que termine la operacion Bluetooth");
  const String device=srv.arg("device");
  if(device!="chihiros"&&device!="pez_verde")return fail(400,"Elige una pantalla valida");
  const uint8_t index=device=="chihiros"?1:0;
  if(!cielo96::ble_release_request(index))return fail(503,"No se pudo solicitar la liberacion");
  record("BLE_CEDER",device+" liberacion solicitada sin ordenes RGB");
  ok();
 });
 srv.on("/api/lab/preview",HTTP_POST,[]{
  if(rescueOpen)return fail(403,"Rescate solo lectura");
  if(!labTxEnabled()||srv.arg("confirm")!="ENSAYO_CURVA")return fail(423,"Autoriza primero el ensayo de curva (90 s)");
  if(!timeOK()||!cielo96::profileValid(profile))return fail(503,"Hora o curva sin validar");
  const String screen=srv.arg("screen");
  const String moment=srv.arg("moment");
  int minute=-1;
  if(moment=="amanecer")minute=900;
  else if(moment=="mediodia")minute=1040;
  else if(moment=="atardecer")minute=1340;
  if(minute<0)return fail(400,"Selecciona amanecer, mediodia o atardecer");
  if(screen=="pez_verde"){
   auto v=cielo::at(profile.pv,profile.pvCount,minute);
   pezManual=true;pezManualUntil=millis()+20000UL;
   submitPv(v,20000);
   record("CURVA_PREVIA_PV",moment+" RGB "+String(v.r)+"/"+String(v.g)+"/"+String(v.b));
  }else if(screen=="chihiros"){
   auto v=cielo::at(profile.ch,profile.chCount,minute);
   chiManual=true;chiManualUntil=millis()+8000UL;
   submitCh(v,8000);
   record("CURVA_PREVIA_CH",moment+" RGB "+String(v.r)+"/"+String(v.g)+"/"+String(v.b));
  }else return fail(400,"Pantalla no valida");
  ok();
 });
 srv.on("/api/lab/tx/arm",HTTP_POST,[]{
  if(rescueOpen)return fail(403,"Rescate es solo lectura");
  if(srv.arg("confirm")!="HABILITAR_MANUAL")return fail(403,"Confirmacion explicita requerida");
  if(!bleReady||!wifiCoexistReady())return fail(423,"Bluetooth o coexistencia no preparados");
  lab.txArmed=true;lab.txUntil=millis()+LAB_TX_WINDOW_MS;lab.txArmCount++;
  record("LAB_TX_ARMADA","Solo ordenes manuales 90 segundos; scheduler desconectado");ok();
 });
 srv.on("/api/lab/tx/disarm",HTTP_POST,[]{
  lab.txArmed=false;record("LAB_TX_DESARMADA","Transmision manual bloqueada");ok();
 });
 srv.on("/api/lab/browser-sample",HTTP_POST,[]{
  if(rescueOpen)return fail(403,"Rescate es solo lectura");
  unsigned count=0,avg=0,maximum=0;
  if(!parseBoundedUInt(srv.arg("count"),count,40)||count<1||
     !parseBoundedUInt(srv.arg("avg"),avg,60000)||
     !parseBoundedUInt(srv.arg("max"),maximum,60000))return fail(400,"Muestra de latencia incorrecta");
  lab.browserSamples+=count;lab.browserAvgMs=avg;
  if(maximum>lab.browserMaxMs)lab.browserMaxMs=maximum;
  record("LAB_HTTP","Latencia desde navegador media "+String(avg)+" ms max "+String(maximum)+" ms");ok();
 });
 srv.on("/api/service/status",HTTP_GET,[]{jsonSend(200,String("{\"version\":")+jsonQuote(VERSION)+",\"otaEnabled\":"+String(CIELO_ENABLE_OTA?"true":"false")+",\"otaPaired\":"+String(prefs.getBytesLength("ota_sha")==32?"true":"false")+",\"otaGrantSeconds\":"+String(physicalOtaGrant()?(otaPhysicalUntil-millis())/1000UL:0)+",\"otaBusy\":"+String(otaBusy?"true":"false")+",\"otaBytes\":"+String(otaBytes)+"}");});
 srv.on("/api/clock",HTTP_POST,[]{if(rescueOpen)return fail(403,"Rescate solo lectura");unsigned epoch;if(!positiveInt(srv.arg("epoch"),epoch)){
   // epoch exceeds minute parser max, parse separately with strict digits
   String s=srv.arg("epoch");if(s.length()<10||s.length()>11)return fail(400,"Epoch incorrecto");
   for(unsigned i=0;i<s.length();i++)if(s[i]<'0'||s[i]>'9')return fail(400,"Epoch incorrecto");
   unsigned long long val=strtoull(s.c_str(),nullptr,10);if(val<1700000000ULL||val>4102444800ULL)return fail(400,"Epoch fuera de rango");
   struct timeval tv{(time_t)val,0};if(settimeofday(&tv,nullptr)!=0)return fail(500,"No se pudo fijar el reloj");phoneSyncedThisBoot=true;record("HORA_SYNC","Reloj desde telefono");return ok();
  }return fail(400,"Epoch demasiado corto");});
 srv.on("/api/save",HTTP_POST,[]{if(rescueOpen)return fail(403,"Rescate solo lectura");updateCurve(true,false);});
 srv.on("/api/reset",HTTP_POST,[]{if(rescueOpen)return fail(403,"Rescate solo lectura");updateCurve(true,true);});
 srv.on("/api/chihiros/curve",HTTP_POST,[]{if(rescueOpen)return fail(403,"Rescate solo lectura");updateCurve(false,false);});
 srv.on("/api/chihiros/curve/reset",HTTP_POST,[]{if(rescueOpen)return fail(403,"Rescate solo lectura");updateCurve(false,true);});
 srv.on("/api/manual",HTTP_POST,[]{if(rescueOpen)return fail(403,"Rescate solo lectura");manualPv();});
 srv.on("/api/chihiros/manual",HTTP_POST,[]{if(rescueOpen)return fail(403,"Rescate solo lectura");manualCh();});
 srv.on("/api/chihiros/stop",HTTP_POST,[]{
  record("CH_AUTO_INSEGURO","Solicitud rechazado: 0x12 activa escenas y produjo apagado fisico");
  return fail(423,"AUTO Chihiros bloqueado: el modo 0x12 apaga DYSL60; restaura desde My Chihiros");
 });
 srv.on("/api/chihiros/evening/off",HTTP_POST,[]{if(rescueOpen)return fail(403,"Rescate solo lectura");return fail(423,"Programador detenido en R4.4; usa Laboratorio para AUTO manual");});
 srv.on("/api/chihiros/evening/on",HTTP_POST,[]{if(rescueOpen)return fail(403,"Rescate solo lectura");return fail(423,"Programador detenido en R4.4; activacion pendiente de pruebas opticas");});
 srv.on("/api/chihiros/live/stop",HTTP_POST,[]{if(rescueOpen)return fail(403,"Rescate solo lectura");return fail(423,"Usa Laboratorio con autorizacion para AUTO manual");});
 srv.on("/api/wifi",HTTP_POST,[]{if(rescueOpen)return fail(403,"Rescate solo lectura");configureWifi();});
 srv.on("/api/wifi/forget",HTTP_POST,[]{if(rescueOpen)return fail(403,"Rescate solo lectura");prefs.remove("ssid");prefs.remove("wifi_pw");storedSSID="";storedPass="";WiFi.disconnect(false,false);ok();});
 srv.on("/api/ota/pair",HTTP_POST,[]{if(rescueOpen)return fail(403,"Rescate solo lectura");otaPair();});
 srv.on("/api/ota/upload",HTTP_POST,[]{
  if(rescueOpen)return fail(403,"Rescate solo lectura");
  startOtaResponse();
 },otaUpload);
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
 if(w==WL_CONNECTED){
  if(!wifiCoexistReady() && now-lastStatusAt>15000UL){
   lastStatusAt=now;
   esp_err_t rc=esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
   record(rc==ESP_OK?"COEX_REPARADA":"COEX_ERROR",rc==ESP_OK?"MIN_MODEM activado":"No se pudo activar MIN_MODEM");
  }
  return;
 }
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
  if(contrastStage==1 && res.device==1 &&
     res.action==static_cast<uint8_t>(cielo::ble::Action::Apply)){
   contrastStage=res.success?2:3;
   contrastManualAt=now;
   record(res.success?"CH_CONTRASTE_30_GATT_OK":"CH_CONTRASTE_30_GATT_FALLO",
     String("30/30/30: ")+String(res.reason)+" "+String(res.duration_ms)+"ms; cambio visual por verificar");
   if(!res.success)lab.txArmed=false;
  }
  if(res.duration_ms>bleDurationMax)bleDurationMax=res.duration_ms;
  if(res.duration_ms>3500UL){bleSlowCount++;record("BLE_LENTO",String(res.device?"CH ":"PV ")+String(res.duration_ms)+"ms");}
  bool autoResult=(res.device==1 && res.action==static_cast<uint8_t>(cielo::ble::Action::RestoreAuto));
  if(!res.success){
   bleResultErrors++;lastBleError=String(res.device?"CH ":"PV ")+String(res.reason);record("BLE_ERROR",lastBleError);
   if(autoResult){chiAutoPending=false;record("CH_AUTO_ERROR","AUTO no restaurado: fallo GATT");}
  }else if(autoResult){
   chiAutoPending=false;record("CH_AUTO","GATT completado, efecto optico no verificado");
  }
 }
 queue.tick(now);
 const uint32_t timeoutNow=queue.metrics().soft_timeouts;
 if(timeoutNow>bleSoftTimeouts){bleSoftTimeouts=timeoutNow;record("BLE_SOFT_TIMEOUT","Web sigue activo; tarea BLE no liberada artificialmente");}
 cielo::ble::Command cmd{};
 if(queue.start_next(now,cmd)){
  if(!labTxEnabled()){
    if(cmd.device==cielo::ble::Device::Chihiros && cmd.action==cielo::ble::Action::RestoreAuto){
     chiAutoPending=false;record("CH_AUTO_NO_ENVIADO","AUTO bloqueado; accion fisica pendiente");
    }
    queue.complete(cmd.sequence,false,now);
    lab.txBlocked++;record("LAB_TX_BLOQUEADA","Cola BLE abortada: autorizacion caducada");
  }else if(!cielo96::ble_dispatch(cmd)){
    queue.complete(cmd.sequence,false,now);record("BLE_COLA_LLENA","No se envio comando al worker");
  }
 }
}
void setup(){
 bootMs=millis();Serial.begin(115200);delay(100);
 uint32_t previousPhaseCode=rtcTrace.phase;
 lastResetTraceValid=(rtcTrace.magic==RTC_TRACE_MAGIC &&
  rtcTrace.check==(RTC_TRACE_MAGIC ^ RTC_TRACE_SALT ^ previousPhaseCode));
 if(lastResetTraceValid)lastResetTracePhase=previousPhaseCode;
 tracePhase(TracePhase::Setup);
 Serial.printf("IWAGUMI CIELO %s | ATOMS3-Lite | build experimental\n",VERSION);
 tracePhase(TracePhase::Nvs);
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
 const String previousOtaApp=prefs.getString("ota_prev","");
 if(running && (previousOtaApp=="app0"||previousOtaApp=="app1") &&
    previousOtaApp!=String(running->label)){
  const uint8_t attempts=prefs.getUChar("ota_attempts",0)+1;
  prefs.putUChar("ota_attempts",attempts);
  Serial.printf("CIELO OTA candidato: intento de arranque %u; version %s\n",attempts,VERSION);
  if(attempts>=3){
   const esp_partition_subtype_t back=previousOtaApp=="app0"?
    ESP_PARTITION_SUBTYPE_APP_OTA_0:ESP_PARTITION_SUBTYPE_APP_OTA_1;
   const esp_partition_t *safe=esp_partition_find_first(ESP_PARTITION_TYPE_APP,back,nullptr);
   if(safe && esp_ota_set_boot_partition(safe)==ESP_OK){
    Serial.printf("CIELO OTA: 3 arranques sin validar; volver a %s\n",previousOtaApp.c_str());
    delay(150);ESP.restart();
   }
  }
 }
 const esp_partition_t *next=esp_ota_get_next_update_partition(nullptr);
 otaRunningPartition=running?running->label:"desconocida";
 otaNextPartition=next?next->label:"NO_DISPONIBLE";
 tracePhase(TracePhase::Profile);
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
 tracePhase(TracePhase::WifiInit);
 WiFi.mode(WIFI_AP_STA);
 // La politica por defecto es MIN_MODEM; configurar despues de arrancar WiFi.
 // WiFi.setSleep(true) antes de softAP/STA producia aviso prematuro.
 WiFi.setAutoReconnect(false);
 WiFi.setHostname("iwagumi-cielo96");
 wifiApOn=WiFi.softAP("CIELO96-CONFIG",apPassword.c_str());
 apProtected=wifiApOn;
 if(storedSSID.length()){WiFi.begin(storedSSID.c_str(),storedPass.c_str());lastWiFiAttempt=millis();}
 if(!wifiCoexistReady()){
  esp_err_t coex=esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
  if(coex!=ESP_OK)record("COEX_PENDIENTE","Se verificara de nuevo al conectar STA");
 }
 tracePhase(TracePhase::Clock);
 configTzTime(CIELO_TZ,"pool.ntp.org","time.google.com");
 bleReady=false; // Do not start BLE until explicit physical authorization.
 record("INICIO",String("reset=")+String((unsigned)esp_reset_reason())+" ("+resetReasonName(esp_reset_reason())+") BLE="+String(bleReady));
 if(abnormalReset(esp_reset_reason()))record("REINICIO_ANOMALO",resetReasonName(esp_reset_reason()));
 Serial.printf("WIFI de rescate si fuera necesaria: CIELO96-CONFIG | clave: %s\n",apPassword.c_str());
 tracePhase(TracePhase::WebInit);
 configureRoutes();
 record("WEB_PRIMERO","HTTP y AP de recuperación inicializados; Bluetooth en espera");
 tracePhase(TracePhase::Idle);
}
void loop(){
 if(!currentBootMarkedHealthy && millis()-bootMs>=180000UL){
  currentBootMarkedHealthy=true;
  prefs.putBool("clean",true);
  if(prefs.getString("ota_prev","").length()){
   prefs.remove("ota_prev");prefs.putUChar("ota_attempts",0);
   record("OTA_ARRANQUE_VALIDADO","Nuevo firmware activo tras 180 segundos; cambios confirmados");
  }
  record("ARRANQUE_ESTABLE","180 segundos sin reiniciar; NO implica que BLE funcione correctamente");
 }
 tracePhase(TracePhase::Http);srv.handleClient();
 tracePhase(TracePhase::Button);physicalButtonTask();
 tracePhase(TracePhase::WifiTick);wifiTask();
 // Arranque BLE automatico SIN transmisiones y solo tras 30 segundos con WiFi STA
 // listo. Se mantiene boton manual de respaldo (3–7 s) y rescate >=8 s.
 if(!blePilotRequested && !blePilotAttempted && !bleReady && !rescueOpen &&
    millis()-bootMs>=30000UL && WiFi.status()==WL_CONNECTED && wifiCoexistReady()){
  blePilotRequested=true;
  record("BLE_AUTO_SOLICITADO","WiFi listo, 30s desde arranque; solo inicializar radio");
 }
 if(blePilotRequested && !blePilotAttempted && !rescueOpen){
  tracePhase(TracePhase::Ble);
  blePilotRequested=false;
  // Do not create the BLE worker unless WiFi's coexistence requirement holds.
  // Do not mark pilot as attempted if the guard blocked its creation.
  if(!wifiCoexistReady())WiFi.setSleep(true);
  if(!wifiCoexistReady()){
   record("BLE_COEX_BLOQUEADO","WiFi modem sleep obligatorio en ESP-IDF 4.4; sin iniciar BLE");
  }else{
   blePilotAttempted=true;
   record("BLE_COEX_VALIDADO",wifiSleepName());
   record("BLE_PILOTO_INICIO","Se prueba Bluetooth con HTTP ya activo");
   bleReady=cielo96::ble_begin();
   record(bleReady?"BLE_PILOTO_PREPARADO":"BLE_PILOTO_ERROR",bleReady?"Tarea BLE creada":"No se pudo crear tarea BLE");
  }
 }
 // Invariante R4.4: iniciar Bluetooth NO habilita el programador de luces.
 // Pruebas de curvas se ejecutan solo en RAM en labTick().
 if(bleReady && !rescueOpen){tracePhase(TracePhase::Ble);bleTick();}
 labTick();
 tracePhase(TracePhase::Led);statusLedTick();
 tracePhase(TracePhase::Idle);delay(3);
}
