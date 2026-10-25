#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
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
#include "cielo_curve_policy.h"
#include "cielo_ble_queue.h"
#include "cielo_ble_worker.h"
#include "cielo_ble_presence.h"
#include "cielo_night_safety.h"
#include "cielo_security_policy.h"
#include "cielo_diagnostic_policy.h"
#include "cielo_ui.h"
#include "cielo_pwa_assets.h"
#include "cielo_monitor_service.h"
#include "cielo_maintenance.h"
#include "cielo_ble_recovery.h"
#include "cielo_ble_blockage.h"
#include "cielo_ble_selective.h"
#include "cielo_pv_day_recovery.h"
#include "cielo_ota_validation.h"
#include "cielo_autonomy_policy.h"
#include "cielo_restart_journal.h"
#include "cielo_cadence_diagnostics.h"

// TLS is not started in this LAN edition. Keep an explicit stack margin for HTTP/OTA.
static constexpr size_t CIELO_LOOP_STACK_BYTES=16384;
SET_LOOP_TASK_STACK_SIZE(CIELO_LOOP_STACK_BYTES);

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
#define CIELO_ENABLE_OTA 1 // OTA LAN sin clave; conserva compatibilidad, integridad y retorno de arranque
#endif

static constexpr const char* VERSION="11.1.0-RC5-RECOVERY3";
static constexpr const char* CIELO_TZ="CET-1CEST,M3.5.0,M10.5.0/3";
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

// Conservado solo entre reinicios de software; nunca borrar NVS para recuperar BLE.
RTC_NOINIT_ATTR static cielo96::recovery::Retained bleRecoveryRtc;
static cielo96::recovery::Supervisor bleSupervisor;
static bool bleRecoveryBoot=false,bleRecoveryBootPvSafe=false,bleRecoveryExhausted=false;
static WebServer srv(80);
static Preferences prefs;
static cielo96::ProfileImage profile;
static cielo::ble::Scheduler queue(3500,250,2000);
static bool bleReady=false, profileRecovery=false, wifiApOn=false;
static bool mdnsReady=false;
static uint32_t mdnsLastAttempt=0;
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
static bool cieloPvRun=false,cieloChRun=false,cieloChWasDriven=false;
static bool cieloAlwaysOn=false,cieloResumePending=false;
static cielo96::autonomy::Retry chReconnect;
static bool chSelectiveHold=false;
static uint32_t chSelectiveEpisodes=0;
enum class PvSafetyState:uint8_t {Paused,Active,Degraded,OffPending,OffRetrying,OffGattOk};
static PvSafetyState pvSafety=PvSafetyState::Paused;
static bool pvOffObligation=false,pvControlDesired=false;
static bool pvDayRecoveryPending=false;
static uint32_t pvDayRecoveryStarted=0,pvDayRecoveryAttempts=0,pvDayRecoveryGattOk=0;
static uint32_t pvOffNextTry=0,pvOffLastTry=0,pvOffLastGatt=0,pvOffAttempts=0,pvOffScheduled=0,pvOffFailures=0,pvRetryNext=0,pvOffInFlightSeq=0;
static const char* pvSafetyName(){switch(pvSafety){case PvSafetyState::Active:return "ACTIVA";case PvSafetyState::Degraded:return "DEGRADADA";case PvSafetyState::OffPending:return "APAGADO_PENDIENTE";case PvSafetyState::OffRetrying:return "REINTENTANDO";case PvSafetyState::OffGattOk:return "APAGADO_GATT_OK";default:return "PAUSADA";}}

static uint32_t cieloDaySamples=0,cieloNightSamples=0;
static uint32_t cieloLastSuccessfulPvAt=0,cieloLastSuccessfulChAt=0;
struct CieloTrendSample {uint32_t epoch=0,pv=0,ch=0,pvErrors=0,chErrors=0,heap=0;};
static CieloTrendSample cieloTrend[96]{};
static uint8_t cieloTrendCount=0,cieloTrendHead=0;
static uint32_t cieloTrendLast=0;
static void cieloTrendTick();
static bool cieloChReturnPending=false,cieloChAwaitResponse=false;
static bool cieloWatchdogTripped=false;
static uint8_t cieloChConsecutiveFailures=0,cieloPvConsecutiveFailures=0,cieloReturnTries=0;
static uint32_t cieloReturnRetryAt=0,cieloStartedAt=0;
static uint32_t cieloPvOrders=0,cieloChOrders=0,cieloPvConfirmed=0,cieloChConfirmed=0;
static uint32_t cieloReturnOk=0,cieloReturnFailed=0,cieloFallbacks=0;
static String cieloReturnReason="Sin solicitar";
static void cieloRequestReturn(const char* reason);
static const char* chSafetyName(){
 if(cieloChReturnPending||cieloChAwaitResponse)return "CONTROL_CEDIENDOSE";
 if(cieloChRun)return "ACTIVA";
 if(cieloChWasDriven)return "CONTROL_CEDIDO_GATT_PENDIENTE";
 return "CONTROL_CEDIDO";
}
static uint32_t pezManualUntil=0,chiManualUntil=0;
static uint32_t lastScheduleAt=0,lastWiFiAttempt=0,lastStatusAt=0,otaBytes=0,otaLastChunkAt=0;
static uint32_t lastPvDesiredAt=0,lastChDesiredAt=0;
static bool cadenceTrial15=false;
static uint32_t cadenceEndsAt=0,cadenceStartBleErrors=0,cadenceTicks15=0,cadenceTicks30=0;
static uint32_t cadenceSkippedBusy=0,cadenceFallbacks=0;
static cielo96::cadence::Diagnostics cadenceDiagnostics;
static String cadenceFallbackReason="Sin ensayo";
static uint32_t activeCadenceMs(){return cadenceTrial15?15000UL:30000UL;}
static int secondOfDay(){time_t t=time(nullptr);struct tm x{};localtime_r(&t,&x);return (x.tm_hour*60+x.tm_min)*60+x.tm_sec;}

static uint32_t httpCount=0,bleResultCount=0,bleResultErrors=0,bleSoftTimeouts=0;
static uint32_t bleSlowCount=0,bleDurationMax=0,wifiFailures=0;
struct DevTelemetry {uint32_t completed=0,gatt_ok=0,write_calls=0,errors=0,slow=0,last_ms=0,max_ms=0;};
static DevTelemetry devices[2]; // PV and Chihiros. GATT acceptance is not an optical ACK.
static bool lastPvDesired=false,lastChDesired=false,chiAutoAttempted=false;
static cielo::PvValue prevPv{};
static cielo::ChiValue prevCh{};
static bool otaBusy=false,otaAllowed=false,otaSuccess=false,otaError=false;
enum class OtaPhase : uint8_t { Idle, Preparing, Ready, Uploading, Completed, Failed };
struct OtaTransaction {
 OtaPhase phase=OtaPhase::Idle;
 uint32_t started=0;
 bool wasPv=false,wasCh=false,wasAlwaysOn=false;
 bool returnAsked=false,releaseAsked=false;
 String reason="Sin preparacion OTA";
};
static OtaTransaction otaTxn{};
static bool otaCandidateBoot=false,otaCandidateNeedsGATT=false;
static cielo96::ota::HealthyWindow otaHealthyWindow;
static uint8_t otaPrefix[cielo96::ota::prefixBytes]{};
static void otaPreparationTick();
static void otaAbortPreparation(const char* reason);
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

static cielo96::restarts::Journal restartJournal;
static bool restartJournalDirty=false;
static uint32_t restartJournalSavedAt=0;
static uint32_t bootEpoch=0;
static void flushRestartJournal(){
 if(!restartJournalDirty||uint32_t(millis()-restartJournalSavedAt)<300000UL)return;
 restartJournalSavedAt=millis();
 cielo96::restarts::seal(restartJournal);
 if(prefs.putBytes("restart_v2",&restartJournal,sizeof(restartJournal))==sizeof(restartJournal))restartJournalDirty=false;
}
static uint32_t savedFailures=0;
struct BootRecord {uint32_t id;uint8_t cause;uint8_t previous_clean;uint16_t reserved;};
static BootRecord bootHistory[8]{};
static uint8_t bootHistoryCount=0,bootHistoryHead=0;
struct JournalEntry {uint32_t boot_id;uint32_t utc_epoch;char code[28];uint16_t repeats;uint16_t reserved;};
struct JournalStore {uint32_t magic;uint8_t count;uint8_t head;uint16_t reserved;JournalEntry entries[8];};
static constexpr uint32_t JOURNAL_MAGIC=0xC1E10505UL;
static JournalStore journal{JOURNAL_MAGIC,0,0,0,{}};
static bool journalDirty=false,journalHasFlushed=false;
static uint32_t journalLastFlush=0;
static void flushJournal(){
 if(!journalDirty || !cielo96::diag::shouldFlush(millis(),journalLastFlush,journalHasFlushed))return;
 journalHasFlushed=true;journalLastFlush=millis(); // Respetar límite incluso si NVS falla.
 if(prefs.putBytes("diag_journal",&journal,sizeof(journal))==sizeof(journal)){
  prefs.putUInt("fault_count",savedFailures);
  journalDirty=false;
 }
}

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
 (void)detail; // Importante: NUNCA persistir SSID, claves, IP ni detalles arbitrarios.
 if(!cielo96::diag::importantEvent(name))return;
 savedFailures++;
 JournalEntry& latest=journal.entries[(journal.head+7)%8];
 if(journal.count && latest.boot_id==previousBootCount+1 &&
    strncmp(latest.code,name,sizeof(latest.code))==0){
  if(latest.repeats<65535)latest.repeats++;
 }else{
  JournalEntry& e=journal.entries[journal.head];e=JournalEntry{};
  e.boot_id=previousBootCount+1;
  time_t t=time(nullptr);e.utc_epoch=t>1700000000L?(uint32_t)t:0;
  snprintf(e.code,sizeof(e.code),"%s",name);e.repeats=1;
  journal.head=(journal.head+1)%8;
  if(journal.count<8)journal.count++;
 }
 journalDirty=true;flushJournal();
}
static void record(const char*name,const String& detail){
 Event&e=events[eventHead];e.elapsed=millis()-bootMs;
 snprintf(e.label,sizeof(e.label),"%s",name);
 snprintf(e.detail,sizeof(e.detail),"%s",detail.c_str());
 eventHead=(eventHead+1)%EVENT_MAX;if(eventCount<EVENT_MAX)eventCount++;lastEventId++;
 Serial.printf("CIELO EVENT %s %s\n",name,detail.c_str());
 if(cielo96::diag::importantEvent(name))persistCritical(name,detail);
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
static void cieloTrendTick(){
 if(!cieloAlwaysOn || !timeOK())return;
 uint32_t now=millis();
 if(cieloTrendLast && now-cieloTrendLast<900000UL)return;
 cieloTrendLast=now;
 auto &t=cieloTrend[cieloTrendHead];t.epoch=uint32_t(time(nullptr));
 t.pv=cieloPvConfirmed;t.ch=cieloChConfirmed;
 t.pvErrors=devices[0].errors;t.chErrors=devices[1].errors;
 t.heap=heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
 cieloTrendHead=(cieloTrendHead+1)%96;
 if(cieloTrendCount<96)cieloTrendCount++;
 time_t ep=time(nullptr);struct tm local{};localtime_r(&ep,&local);
 const int minutes=local.tm_hour*60+local.tm_min;
 if(minutes<870 || minutes>=1380)cieloNightSamples++;else cieloDaySamples++;
}
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
static cielo::PvValue desiredPv(){return timeOK()?(cadenceTrial15?cielo::atSecond(profile.pv,profile.pvCount,secondOfDay()):cielo::at(profile.pv,profile.pvCount,minuteOfDay())):cielo::PvValue{0,0,0,0};}
static cielo::ChiValue desiredCh(){return timeOK()?(cadenceTrial15?cielo::atSecond(profile.ch,profile.chCount,secondOfDay()):cielo::at(profile.ch,profile.chCount,minuteOfDay())):cielo::ChiValue{0,0,0};}

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
 const cielo96::ProfileImage* latest=cielo96::curves::newestValid(va?&a:nullptr,vb?&b:nullptr);
 if(latest){profile=*latest;
  record("CURVAS_OK",String("NVS generacion=")+String(profile.generation));
 }else{
  bool corrupt=prefs.getBytesLength("curvesA")!=0 || prefs.getBytesLength("curvesB")!=0;
  profileRecovery=corrupt;
  record(corrupt?"CURVA_ERROR":"CURVAS_INICIALES",corrupt?"NVS invalida; Natural v4 en RAM":"Primera inicializacion; Natural v4 sin copia anterior");
 }
}
static bool saveProfile(const cielo96::ProfileImage& candidate){
 if(!cielo96::curves::curveValid(candidate))return false;
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
static bool positiveInt(const String&s,unsigned& n){
 if(!s.length())return false;uint32_t x=0;
 for(size_t i=0;i<s.length();i++){char c=s[i];if(c<'0'||c>'9')return false;x=x*10+(c-'0');if(x>1440)return false;}
 n=x;return true;
}
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
static bool uint32Strict(const String& s,uint32_t &v){
 if(!s.length()||s.length()>10)return false;
 uint64_t n=0;
 for(size_t i=0;i<s.length();i++){
  if(s[i]<'0'||s[i]>'9')return false;
  n=n*10+(s[i]-'0');if(n>0xffffffffULL)return false;
 }
 v=(uint32_t)n;return true;
}
static void saveBothCurves(){
 if(srv.arg("schema_version")!="2")return fail(400,"Formato incompatible: se requiere schema_version=2");
 uint32_t expected=0;
 if(!uint32Strict(srv.arg("generation"),expected))return fail(400,"Generacion no valida");
 if(!cielo96::curves::expectedGeneration(profile.generation,expected))
  return fail(409,"El perfil ha cambiado en ATOM: recarga antes de guardar");
 const String pv=srv.arg("pv"),ch=srv.arg("ch"),tag=srv.arg("checksum");
 if(pv.isEmpty()||ch.isEmpty()||pv.length()>1700||ch.length()>1700||tag.length()!=8)
  return fail(400,"Curva incompleta o supera limites");
 char digest[9];snprintf(digest,sizeof(digest),"%08lx",(unsigned long)cielo96::curves::wireHash(
    pv.c_str(),pv.length(),ch.c_str(),ch.length()));
 if(!tag.equalsIgnoreCase(digest))return fail(400,"Integridad del perfil incorrecta: nada guardado");
 cielo96::ProfileImage candidate=profile;
 if(!parsePoints(pv,true,candidate)||!parsePoints(ch,false,candidate))
  return fail(400,"Una curva contiene puntos incorrectos: nada guardado");
 candidate.generation=profile.generation+1;
 cielo96::profileSeal(candidate);
 if(!cielo96::curves::curveValid(candidate))return fail(400,"Perfil completo no valido");
 if(!saveProfile(candidate))return fail(507,"No se pudo comprobar la escritura NVS. Perfil previo conservado");
 lastPvDesired=false;lastChDesired=false;
 record("PERFIL_ATOMICO_GUARDADO",String("PV=")+String(profile.pvCount)+" CH="+String(profile.chCount)+" gen="+String(profile.generation));
 jsonSend(200,String("{\"ok\":true,\"generation\":")+String(profile.generation)+",\"pv\":"+String(profile.pvCount)+",\"chi\":"+String(profile.chCount)+"}");
}
static String cadenceJson(){
 String j="{\"interval_ms\":"+String(activeCadenceMs());
 j+=",\"trial_active\":"+String(cadenceTrial15?"true":"false");
 j+=",\"trial_remaining_sec\":"+String(cadenceTrial15?(uint32_t)(cadenceEndsAt-millis())/1000UL:0);
 j+=",\"ticks_15s\":"+String(cadenceTicks15)+",\"ticks_30s\":"+String(cadenceTicks30);
 j+=",\"skipped_busy\":"+String(cadenceSkippedBusy)+",\"automatic_fallbacks\":"+String(cadenceFallbacks);
 j+=",\"busy_counter_definition\":\"skipped_busy is a legacy count of busy checkpoints, not skipped curve updates\"";
 j+=",\"due_ticks\":"+String(cadenceDiagnostics.due);
 j+=",\"busy_inflight_ticks\":"+String(cadenceDiagnostics.busy);
 j+=",\"pending_only_ticks\":"+String(cadenceDiagnostics.pendingOnly);
 j+=",\"long_phase_ticks\":"+String(cadenceDiagnostics.longPhase);
 j+=",\"evaluated_ticks\":"+String(cadenceDiagnostics.evaluated);
 j+=",\"off_priority_ticks\":"+String(cadenceDiagnostics.offPriority);
 j+=",\"invalid_clock_ticks\":"+String(cadenceDiagnostics.invalidClock);
 j+=",\"inhibited_ticks\":"+String(cadenceDiagnostics.inhibited);
 j+=",\"fallback_reason\":"+jsonQuote(cadenceFallbackReason)+",\"optically_verified\":false}";
 return j;
}
static void cadenceSet(){
 if(srv.arg("mode")=="30"){
  cadenceTrial15=false;cadenceFallbackReason="Restaurado manualmente a 30 s";
  lastScheduleAt=millis()-30000UL;
  record("CADENCIA_30S","Ensayo de 15s detenido");return jsonSend(200,cadenceJson());
 }
 if(srv.arg("mode")!="15"||srv.arg("confirm")!="ENSAYO_SUPERVISADO_15S")
  return fail(403,"Activacion de ensayo supervisado requerida");
 if(!bleReady||!timeOK()||!cielo96::curves::curveValid(profile)||!wifiCoexistReady()||otaTxn.phase!=OtaPhase::Idle||!currentBootMarkedHealthy||(!cieloPvRun&&!cieloChRun))
  return fail(423,"Reloj, Bluetooth, OTA o arranque no preparados para 15 s");
 const auto snap=cielo96::ble_worker_snapshot();
 if(!cielo96::curves::secondsModeSafe(true,!cieloWatchdogTripped&&snap.heartbeat_age_ms<5000UL,true,
       queue.busy()||cielo96::ble_request_queue_depth()>0,heap_caps_get_free_size(MALLOC_CAP_INTERNAL),0))
  return fail(423,"Memoria o Bluetooth insuficientes para 15 s");
 cadenceTrial15=true;cadenceEndsAt=millis()+1200000UL;
 cadenceStartBleErrors=bleResultErrors;cadenceFallbackReason="Sin errores; ensayo 20 minutos";
 lastScheduleAt=millis()-30000UL;lastPvDesired=false;lastChDesired=false;
 record("CADENCIA_15S","Prueba limitada a 20 minutos, regreso automatico a 30s");
 jsonSend(200,cadenceJson());
}
static bool bleCommunicationRecent(uint8_t device){
 const auto link=cielo96::ble_link_status(device);
 const uint32_t last=device?cieloLastSuccessfulChAt:cieloLastSuccessfulPvAt;
 const uint32_t errors=device?cieloChConsecutiveFailures:(cieloPvConsecutiveFailures+pvOffFailures);
 return cielo96::presence::recent(link.connected||link.idle_released,last,millis(),errors);
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
 j+=",\"pv_safety_state\":"+jsonQuote(pvSafetyName());
 j+=",\"pv_off_obligation\":"+String(pvOffObligation?"true":"false");
 j+=",\"pv_control_requested\":"+String(pvControlDesired?"true":"false");
 j+=",\"pv_off_attempts\":"+String(pvOffAttempts)+",\"pv_off_scheduled\":"+String(pvOffScheduled)+",\"pv_off_failures\":"+String(pvOffFailures);
 j+=",\"pv_off_last_gatt_ms\":"+String(pvOffLastGatt)+",\"pv_off_last_attempt_ms\":"+String(pvOffLastTry);
 j+=",\"pv_optically_confirmed\":false";
 j+=",\"pv_day_recovery_pending\":"+String(pvDayRecoveryPending?"true":"false");
 j+=",\"pv_day_recovery_attempts\":"+String(pvDayRecoveryAttempts);
 j+=",\"pv_day_recovery_gatt_ok\":"+String(pvDayRecoveryGattOk);
 j+=",\"ch_consecutive_errors\":"+String(cieloChConsecutiveFailures);
 j+=",\"ch_safety_state\":"+jsonQuote(chSafetyName());
 j+=",\"started_at_ms\":"+String(cieloStartedAt);
 j+=",\"pv_last_requested\":["+String(prevPv.r)+","+String(prevPv.g)+","+String(prevPv.b)+","+String(prevPv.brightness)+"]";
 j+=",\"ch_last_requested\":["+String(prevCh.r)+","+String(prevCh.g)+","+String(prevCh.b)+"]";
 j+=",\"pv_last_requested_age_sec\":"+String(lastPvDesiredAt?(millis()-lastPvDesiredAt)/1000UL:0);
 j+=",\"ch_last_requested_age_sec\":"+String(lastChDesiredAt?(millis()-lastChDesiredAt)/1000UL:0);
 j+=",\"pv_last_gatt_age_sec\":"+(cieloLastSuccessfulPvAt?String((millis()-cieloLastSuccessfulPvAt)/1000UL):String("null"));
 j+=",\"ch_last_gatt_age_sec\":"+(cieloLastSuccessfulChAt?String((millis()-cieloLastSuccessfulChAt)/1000UL):String("null"));
 j+=",\"ch_window_now\":"+String(timeOK()?"true":"false");
 j+=",\"ch_reconnect_pending\":"+String(chReconnect.recovering?"true":"false");
 j+=",\"ch_reconnect_attempts\":"+String(chReconnect.attempts);
 j+=",\"ch_reconnect_episodes\":"+String(chReconnect.episodes);
 j+=",\"always_on\":"+String(cieloAlwaysOn?"true":"false");
 j+=",\"pv_link_idle_released\":"+String(cielo96::ble_link_status(0).idle_released?"true":"false");
 j+=",\"ch_link_idle_released\":"+String(cielo96::ble_link_status(1).idle_released?"true":"false");
 j+=",\"pv_link_connected\":"+String(cielo96::ble_link_status(0).connected?"true":"false");
 j+=",\"ch_link_connected\":"+String(cielo96::ble_link_status(1).connected?"true":"false");
 j+=",\"communication_fresh_limit_sec\":90";
 j+=",\"pv_online_recent\":"+String(cieloPvRun&&bleCommunicationRecent(0)?"true":"false");
 j+=",\"ch_online_recent\":"+String(cieloChRun&&bleCommunicationRecent(1)?"true":"false");
 j+=",\"day_samples\":"+String(cieloDaySamples)+",\"night_samples\":"+String(cieloNightSamples);
 j+=",\"return_stored_after_reboot\":"+String(prefs.getBool("v4_ch_takeover",false)?"true":"false");
 j+=",\"note\":\"La recuperacion 0x03 requiere BLE operativo y energia del ATOM\"}";
 return j;
}
static String cieloTrendJson(){
 String j;j.reserve(9000);
 j="{\"interval_min\":15,\"capacity\":96,\"samples\":"+String(cieloTrendCount);
 j+=",\"day_samples\":"+String(cieloDaySamples)+",\"night_samples\":"+String(cieloNightSamples);
 j+=",\"history\":[";
 for(uint8_t n=0;n<cieloTrendCount;n++){
  const auto &v=cieloTrend[(cieloTrendHead+96-cieloTrendCount+n)%96];
  if(n)j+=",";
  j+="["+String(v.epoch)+","+String(v.pv)+","+String(v.ch)+","+String(v.pvErrors)+","+String(v.chErrors)+","+String(v.heap)+"]";
 }
 j+="]}";return j;
}
static void statusJson(){
 auto p=desiredPv();auto c=desiredCh();bool w=WiFi.status()==WL_CONNECTED;
 String j;j.reserve(4500);
 j="{\"firmware\":\""+String(VERSION)+"\",\"ble\":"+String(bleReady?"true":"false")+",\"clockValid\":"+String(timeOK()?"true":"false");
 j+=",\"clock\":"+jsonQuote(dateTime())+",\"wifi\":"+(w?String("true"):String("false"));
 j+=",\"ip\":\"192.168.4.1\",\"homeIp\":"+jsonQuote(w?WiFi.localIP().toString():String(""));
 j+=",\"homeSsid\":"+jsonQuote(w?WiFi.SSID():storedSSID);
 j+=",\"wifiRssi\":"+String(w?WiFi.RSSI():0);
 j+=",\"deviceClockEpoch\":"+String(uint32_t(time(nullptr)));
 j+=",\"wifiStatus\":"+jsonQuote(w?"Router conectado":"No conectado")+",\"wifiConnecting\":false,\"wifiStage\":"+jsonQuote(w?"CONECTADA":"SIN_RED");
 j+=",\"manual\":"+String(pezManual?"true":"false")+",\"power\":"+String(p.brightness>0?"true":"false");
 j+=",\"rgb\":["+String(p.r)+","+String(p.g)+","+String(p.b)+"],\"brightness\":"+String(p.brightness);
 j+=",\"points\":"+jsonQuote(pointsPv())+",\"chiPoints\":"+jsonQuote(pointsCh());
 j+=",\"profileGeneration\":"+String(profile.generation)+",\"profileSchemaVersion\":2";
 j+=",\"cadence\":"+cadenceJson();
 j+=",\"cieloV4\":"+cieloModeJson();
 j+=",\"chActive\":"+String(cieloChRun?"true":"false")+",\"chEvening\":"+String(cieloChRun?"true":"false")+",\"chContinuous\":"+String(cieloChRun?"true":"false");
 j+=",\"chStatus\":"+jsonQuote(cieloChRun?"Natural V4: control por CIELO":cieloChReturnPending||cieloChAwaitResponse?"Restaurando curva de My Chihiros":"My Chihiros: curva personalizada");
 j+=",\"chEveningStatus\":"+jsonQuote(cieloChRun?"NATURAL_V4_ACTIVA":"MY_CHIHIROS")+",\"bleFailures\":"+String(bleResultErrors);
 j+=",\"txFrames\":"+String(devices[0].write_calls)+",\"txBatches\":"+String(devices[0].completed);
 j+=",\"bleError\":"+jsonQuote(lastBleError)+",\"bleLive\":false,\"storm\":false,\"stormStatus\":\"No disponible en R4 DEV\"";
 j+=",\"chFrames\":"+String(devices[1].write_calls)+",\"chFailures\":"+String(devices[1].errors);
 j+=",\"chLiveRgb\":["+String(c.r)+","+String(c.g)+","+String(c.b)+"],\"chLiveFailures\":0,\"chAutoAttempts\":"+String(chiAutoAttempted?1:0);
 j+=",\"otaPaired\":"+String("false")+"}";
 jsonSend(200,j);
}
static bool otaNetworkReady();
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
  j+=",\"idle_released\":"+String(link.idle_released?"true":"false");
  j+=",\"connection_pending\":"+String(link.connection_pending?"true":"false");
  j+=",\"pending_age_ms\":"+String(link.pending_age_ms);
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
  if(cieloChAwaitResponse){
   cieloChAwaitResponse=false;
   if(custom.success){
    cieloReturnOk++;cieloChWasDriven=false;lastChDesired=false;
    cieloReturnTries=0;cieloChReturnPending=false;
    if(!prefs.putBool("v4_ch_takeover",false))
     record("V4_NVS_AVISO","No se pudo limpiar marca de control anterior");
    cieloReturnReason="Curva personalizada My Chihiros: GATT OK (efecto optico no medido)";
    record("V4_RETORNO_GATT_OK",cieloReturnReason);
   }else{
    cieloReturnFailed++;
    if(cieloReturnTries<2){
     cieloChReturnPending=true;cieloReturnRetryAt=now+8000UL;
     record("V4_RETORNO_REINTENTO","Fallo 0x03; reintento unico despues de 8 segundos");
    }else{
     cieloChReturnPending=false;
     cieloReturnReason="Retorno My Chihiros fallo 2 veces; usar My Chihiros";
     record("V4_RETORNO_FALLO",cieloReturnReason);
    }
   }
  }
 }
 if(customResumePending && now-customResumeRequested>24000UL){
  customResumePending=false;customResumeFailed++;
  customResumeLast="Sin respuesta de la tarea BLE en 24 s; no repetir automaticamente";
  if(contrastStage==4)contrastStage=6;
  if(cieloChAwaitResponse){
   cieloChAwaitResponse=false;cieloReturnFailed++;
   if(cieloReturnTries<2){
    cieloChReturnPending=true;cieloReturnRetryAt=now+8000UL;
   }else{
    cieloChReturnPending=false;cieloReturnReason="Retorno 0x03 agotado, utiliza My Chihiros";
   }
  }
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
static String restartHistoryJson(){
 String j="[";
 for(uint8_t i=0;i<restartJournal.count;i++){
  if(i)j+=",";const auto&e=cielo96::restarts::at(restartJournal,i);
  j+="{\"boot_id\":"+String(e.boot)+",\"previous_boot_id\":"+String(e.previous);
  j+=",\"boot_epoch\":"+String(e.epoch)+",\"reset_reason\":"+jsonQuote(resetReasonName((esp_reset_reason_t)e.reset));
  j+=",\"ble_recovery\":"+String(e.bleRecovery?"true":"false");
  j+=",\"stall_epoch\":"+String(e.stallEpoch)+",\"stall_ms\":"+String(e.stallMs);
  j+=",\"phase\":"+jsonQuote(cielo96::ble_worker_phase_name(e.phase));
  j+=",\"device\":"+String(e.device)+",\"outcome\":"+jsonQuote(e.outcome?"COMMUNICATION_RECOVERED":"PENDING");
  j+=",\"recovered_epoch\":"+String(e.recoveredEpoch)+"}";
 }
 return j+"]";
}
static void diagnosticJson(){
 const auto&m=queue.metrics();bool wifi=WiFi.status()==WL_CONNECTED;
 uint32_t freeMem=heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
 uint32_t minimum=heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
 uint32_t largest=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
 bool badReset=abnormalReset(esp_reset_reason());
 bool workerUnavailable=bleReady && (cielo96::ble_worker_snapshot().heartbeat_age_ms>45000UL);
 bool critical=(badReset&&!currentBootMarkedHealthy)||(!bleReady&&blePilotAttempted)||(!wifi&&!wifiApOn)||
  workerUnavailable||profileRecovery||otaError||!timeOK()||
  (pvOffObligation&&pvOffFailures>=3); // Los errores BLE ya recuperados son historicos.
 bool historical=badReset||m.soft_timeouts||bleSlowCount||savedFailures||bleResultErrors;
 String j;j.reserve(22000);
 j="{\"schema\":\"iwagumi-cielo-96-diagnostico-v3\",\"firmware\":"+jsonQuote(VERSION)+",\"generated_at\":"+jsonQuote(dateTime());
 j+=",\"boot_id\":"+String(previousBootCount+1);
 j+=",\"generated_epoch\":"+(timeOK()?String((uint32_t)time(nullptr)):String("null"));
 j+=",\"share_safe\":"+String(srv.hasArg("share")?"true":"false");
 j+=",\"uptime_sec\":"+String((millis()-bootMs)/1000UL)+",\"status\":"+
  jsonQuote(critical?"incidencia_actual":historical?"avisos_historicos":"sin_avisos_actuales");
 j+=",\"alerts\":{\"current_critical\":"+String(critical?"true":"false");
 j+=",\"historical_soft_timeouts\":"+String(m.soft_timeouts);
 j+=",\"historical_slow_calls\":"+String(bleSlowCount);
 j+=",\"historical_persistent_faults\":"+String(savedFailures);
 j+=",\"meaning\":\"Los avisos historicos no significan que la luz haya fallado ahora\"}";
 j+=",\"pilot\":{\"ble_start_requires_button\":false,\"ble_start_attempted\":"+String(blePilotAttempted?"true":"false")+",\"rescue_read_only\":"+String(rescueOpen?"true":"false")+",\"rescue_seconds\":"+String(rescueOpen?(rescueUntil-millis())/1000UL:0)+",\"private_ap_active\":"+String(apProtected?"true":"false")+"}";
 j+=",\"system\":{\"reset_reason\":"+String((unsigned)esp_reset_reason())+",\"reset_reason_name\":"+jsonQuote(resetReasonName(esp_reset_reason()))+",\"abnormal_reset\":"+String(badReset?"true":"false")+",\"boot_marked_healthy\":"+String(currentBootMarkedHealthy?"true":"false")+",\"watchdog_active\":null";
 j+=",\"ble_recovery\":{\"enabled\":true,\"stall_limit_ms\":120000,\"restart_limit\":2,\"rearm_healthy_ms\":1800000";
 j+=",\"attempts\":"+String(bleRecoveryRtc.attempts)+",\"exhausted\":"+String(bleRecoveryExhausted?"true":"false");
 j+=",\"recovered_boot\":"+String(bleRecoveryBoot?"true":"false")+",\"pv_off_interlock\":"+String(bleRecoveryBootPvSafe?"true":"false");
 j+=",\"last_phase\":"+jsonQuote(cielo96::ble_worker_phase_name(bleRecoveryRtc.phase))+",\"last_device\":"+String(bleRecoveryRtc.device);
 j+=",\"transport\":\"BLUEDROID_2_0_17_BOUNDED\",\"connection_timeout_ms\":8000";
 j+=",\"last_stall_epoch\":"+String(bleRecoveryRtc.epoch);
 j+=",\"last_stall_ms\":"+String(bleRecoveryRtc.age_ms)+",\"optical_confirmation\":false}";
 j+=",\"loop_stack_bytes\":"+String(CIELO_LOOP_STACK_BYTES);
 j+=",\"loop_stack_min_free_bytes\":"+String(uxTaskGetStackHighWaterMark(nullptr));
 j+=",\"heap_free\":"+String(freeMem)+",\"heap_minimum\":"+String(minimum)+",\"largest_heap_block\":"+String(largest);
 j+=",\"heap_metric\":\"MALLOC_CAP_INTERNAL\",\"fragmentation_pct\":"+(freeMem?String(cielo96::diag::fragmentationPct(freeMem,largest)):String("null"));
 j+=",\"fragmentation_definition\":\"100 x (1 - largest_free_block / free_heap), solo estimacion\"";
 j+=",\"filesystem_ok\":null,\"filesystem_errors\":null";
 j+=",\"boot_count\":"+String(previousBootCount+1)+",\"previous_boot_marked_clean\":"+String(priorBootClean?"true":"false");
 j+=",\"last_phase\":"+jsonQuote(tracePhaseName(rtcTrace.phase));
 j+=",\"previous_reset_phase_valid\":"+String(lastResetTraceValid?"true":"false");
 j+=",\"previous_reset_phase\":"+jsonQuote(lastResetTraceValid?tracePhaseName(lastResetTracePhase):"NO_DISPONIBLE");
 j+=",\"previous_reset_phase_is_hint_only\":true";
 j+=",\"persistent_fault_count\":"+String(savedFailures)+",\"prior_fault_type\":"+jsonQuote(priorEvent);
 j+=",\"prior_fault_detail\":"+(srv.hasArg("share")?String("null"):jsonQuote(priorEventDetail))+"}";
 j+=",\"network\":{\"home_wifi_connected\":"+String(wifi?"true":"false")+",\"ap_enabled\":"+String(wifiApOn?"true":"false");
 j+=",\"wifi_disconnects\":"+String(wifiDisconnects)+",\"wifi_connection_attempts\":"+String(wifiFailures)+",\"wifi_rssi_dbm\":"+String(wifi?WiFi.RSSI():0);
 j+=",\"wifi_modem_sleep\":"+jsonQuote(wifiSleepName())+",\"wifi_ble_coex_ready\":"+String(wifiCoexistReady()?"true":"false")+"}";
 j+=",\"lighting\":{\"natural_v4\":"+cieloModeJson();
 j+=",\"pez_verde\":{\"mode\":"+jsonQuote(cieloPvRun?"NATURAL_V4_CIELO":pezManual?"manual":"programa")+",\"tx_batches\":"+String(devices[0].completed);
 j+=",\"tx_frames\":"+String(devices[0].write_calls)+",\"errors\":"+String(devices[0].errors)+"},\"chihiros\":{\"cielo_night_enabled\":"+String(cieloChRun?"true":"false");
 j+=",\"ble_active\":"+String(bleReady?"true":"false")+",\"auto_restore_pending\":"+String(chiAutoPending?"true":"false")+",\"tx_frames\":"+String(devices[1].write_calls)+",\"errors\":"+String(devices[1].errors)+"}}";
 j+=",\"bluetooth\":{\"worker_heartbeat_age_ms\":"+String(cielo96::ble_worker_snapshot().heartbeat_age_ms);
 for(uint8_t linkIndex=0;linkIndex<2;linkIndex++){
  const auto link=cielo96::ble_link_status(linkIndex);
  j+=",\"link_";
  j+=linkIndex?"chihiros":"pez_verde";
  j+="\":{\"connected\":"+String(link.connected?"true":"false");
  j+=",\"idle_released\":"+String(link.idle_released?"true":"false");
  j+=",\"connection_pending\":"+String(link.connection_pending?"true":"false");
  j+=",\"pending_age_ms\":"+String(link.pending_age_ms);
  j+=",\"released_to_app\":"+String(link.released_to_app?"true":"false");
  j+=",\"release_count\":"+String(link.release_count)+"}";
 }
 j+=",\"queued\":"+String(m.submitted)+",\"queued_total_is_submitted\":true";
 j+=",\"queue_depth\":"+String((queue.pending(cielo::ble::Device::PezVerde)?1:0)+(queue.pending(cielo::ble::Device::Chihiros)?1:0));
 j+=",\"worker_queue_depth\":"+String(cielo96::ble_request_queue_depth());
 j+=",\"in_flight\":"+String(queue.busy()?"true":"false");
 j+=",\"superseded\":"+String(m.superseded)+",\"failed\":"+String(m.failed);
 j+=",\"timeouts\":"+String(m.soft_timeouts)+",\"retries\":"+String(m.retries)+",\"max_call_ms\":"+String(bleDurationMax);
 j+=",\"slow_calls\":"+String(bleSlowCount);
 j+=",\"per_device\":{\"pez_verde\":{\"operations\":"+String(devices[0].completed)+",\"gatt_ok\":"+String(devices[0].gatt_ok)+",\"write_calls\":"+String(devices[0].write_calls)+",\"errors\":"+String(devices[0].errors)+",\"slow_calls\":"+String(devices[0].slow)+",\"last_ms\":"+String(devices[0].last_ms)+",\"max_ms\":"+String(devices[0].max_ms)+"}";
 j+=",\"chihiros\":{\"operations\":"+String(devices[1].completed)+",\"gatt_ok\":"+String(devices[1].gatt_ok)+",\"write_calls\":"+String(devices[1].write_calls)+",\"errors\":"+String(devices[1].errors)+",\"slow_calls\":"+String(devices[1].slow)+",\"last_ms\":"+String(devices[1].last_ms)+",\"max_ms\":"+String(devices[1].max_ms)+"}}";
 j+=",\"p95_ms\":{\"pez_verde\":"+(cielo96::diag::canReportP95(latencyCount[0])?String(latencyP95(0)):String("null"));
 j+=",\"chihiros\":"+(cielo96::diag::canReportP95(latencyCount[1])?String(latencyP95(1)):String("null"))+"}";
 j+=",\"p95_min_samples\":10,\"latency_samples\":{\"pez_verde\":"+String(latencyCount[0])+",\"chihiros\":"+String(latencyCount[1])+"}";
 j+=",\"phase_timing_ms\":{";
 for(uint8_t i=0;i<2;i++){
  const auto x=cielo96::ble_transport_stats(i);if(i)j+=",";
  j+=jsonQuote(i?"chihiros":"pez_verde")+":{\"connect\":{\"count\":"+String(x.connect_count)+",\"failed\":"+String(x.connect_failed)+",\"last_ms\":"+String(x.connect_last_ms)+",\"max_ms\":"+String(x.connect_max_ms)+",\"timeout_count\":"+String(x.connect_timeouts)+",\"last_stack_error\":"+String(x.last_stack_error)+"}";
  j+=",\"write\":{\"count\":"+String(x.write_count)+",\"failed\":"+String(x.write_failed)+",\"last_ms\":"+String(x.write_last_ms)+",\"max_ms\":"+String(x.write_max_ms)+"}";
  j+=",\"disconnect\":{\"count\":"+String(x.disconnect_count)+",\"last_ms\":"+String(x.disconnect_last_ms)+",\"max_ms\":"+String(x.disconnect_max_ms)+"}}";
 }j+="}";
 const auto blockageSnapshot=cielo96::ble_blockage_snapshot();
 const auto blockage=cielo96::blockage::classify({
  blockageSnapshot.worker.phase,blockageSnapshot.worker.device,
  blockageSnapshot.worker.heartbeat_age_ms,blockageSnapshot.worker.phase_age_ms,
  blockageSnapshot.pending_pv_age_ms,blockageSnapshot.pending_ch_age_ms});
 j+=",\"blockage_diagnostic\":{\"kind\":"+jsonQuote(cielo96::blockage::name(blockage.primary));
 j+=",\"sdk_connect_pending\":"+String(blockage.sdk_pending?"true":"false");
 j+=",\"pending_device\":"+String(blockage.pending_device);
 j+=",\"pending_age_ms\":"+String(blockage.pending_age_ms);
 j+=",\"gatt_discovery_wait\":"+String(blockage.discovery_wait?"true":"false");
 j+=",\"worker_unresponsive\":"+String(blockage.worker_unresponsive?"true":"false");
 j+=",\"actual_worker_heartbeat_age_ms\":"+String(blockageSnapshot.worker.heartbeat_age_ms);
 j+=",\"actual_worker_phase_age_ms\":"+String(blockageSnapshot.worker.phase_age_ms);
 j+=",\"phase_is_inference\":true,\"selective_ch_hold\":"+String(chSelectiveHold?"true":"false");
 j+=",\"selective_ch_episodes\":"+String(chSelectiveEpisodes);
 j+=",\"rescue_fallback_at_ms\":300000,\"policy\":\"SAFE_CH_SDK_PENDING_ONLY\"}";
 const auto workerSnap=cielo96::ble_worker_snapshot();
 const auto phaseTrace=cielo96::ble_phase_history();
 j+=",\"phase_trace\":{\"volatile\":true,\"count\":"+String(phaseTrace.count)+",\"events\":[";
 for(uint8_t k=0;k<phaseTrace.count;k++){
  if(k)j+=",";
  const auto&e=phaseTrace.events[k];
  j+="["+String(e.ordinal)+","+String(e.at_ms)+","+String(e.device)+","+
     jsonQuote(cielo96::ble_worker_phase_name(e.phase))+"]";
 }
 j+="]}";
 j+=",\"pv_day_recovery\":{\"pending\":"+String(pvDayRecoveryPending?"true":"false");
 j+=",\"attempts\":"+String(pvDayRecoveryAttempts);
 j+=",\"gatt_ok\":"+String(pvDayRecoveryGattOk);
 j+=",\"started_at_ms\":"+String(pvDayRecoveryStarted);
 j+=",\"optical_verified\":false}";
 j+=",\"worker_phase\":"+jsonQuote(cielo96::ble_worker_phase_name(workerSnap.phase));
 j+=",\"worker_phase_age_ms\":"+String(workerSnap.phase_age_ms);
 j+=",\"worker_device\":"+String(workerSnap.device);
 j+=",\"worker_unresponsive\":"+String(workerUnavailable?"true":"false");
 j+=",\"note\":\"La escritura GATT completada no garantiza recepcion optica\"}";
 const bool bleAlive=bleReady&&!workerUnavailable;
 auto pvLevel=cielo96::diag::link(bleReady,bleAlive,devices[0].completed>0,
   bleCommunicationRecent(0),pvOffObligation&&pvSafety!=PvSafetyState::OffGattOk);
 auto chLevel=cielo96::diag::link(bleReady,bleAlive,devices[1].completed>0,
   bleCommunicationRecent(1),cieloChReturnPending||cieloChAwaitResponse);
 if(pvOffObligation&&pvOffFailures>=3)pvLevel=cielo96::diag::Level::Error;
 if(!pvControlDesired&&!cieloPvRun&&!pvOffObligation)pvLevel=cielo96::diag::Level::NotTested;
 if(!cieloChRun&&!cieloChReturnPending&&!cieloChAwaitResponse)chLevel=cielo96::diag::Level::NotTested;
 j+=",\"professional\":{\"levels\":{\"correct\":\"CORRECTO\",\"failed\":\"FALLO\",\"warning\":\"ADVERTENCIA\",\"not_tested\":\"NO_PROBADO\",\"no_data\":\"SIN_DATOS\"}";
 j+=",\"wifi\":"+jsonQuote(wifi?"CORRECTO":"FALLO");
 j+=",\"clock\":"+jsonQuote(timeOK()?"CORRECTO":"SIN_DATOS");
 j+=",\"pez_verde\":"+jsonQuote(cielo96::diag::name(pvLevel));
 j+=",\"chihiros\":"+jsonQuote(cielo96::diag::name(chLevel));
 j+=",\"optical_feedback\":\"NO_PROBADO\"";
 j+=",\"power_off\":{\"required\":"+String(pvOffObligation?"true":"false");
 j+=",\"scheduled\":"+String(pvOffScheduled)+",\"ble_attempts\":"+String(pvOffAttempts);
 j+=",\"failed_attempts\":"+String(pvOffFailures);
 j+=",\"last_attempt_uptime_ms\":"+(pvOffLastTry?String(pvOffLastTry):String("null"));
 j+=",\"last_gatt_ok_uptime_ms\":"+(pvOffLastGatt?String(pvOffLastGatt):String("null"));
 j+=",\"gatt_status\":"+jsonQuote(pvOffLastGatt?"GATT_ACEPTADO":"NO_PROBADO");
 j+=",\"optically_off\":null,\"reason\":\"Una escritura GATT no mide la luz real\"}}";
 j+=",\"curves\":{\"generation\":"+String(profile.generation)+",\"points_pv\":"+String(profile.pvCount);
 j+=",\"points_ch\":"+String(profile.chCount)+",\"recovery_defaults\":"+String(profileRecovery?"true":"false")+",\"last_error\":"+jsonQuote(lastBackupError)+"}";
 j+=",\"cadence\":"+cadenceJson();
 j+=",\"security\":{\"https_ready\":"+String("false")+",\"authentication\":\"NONE_LAN\",\"ota_signature\":\"NOT_REQUIRED_OWNER_REQUEST\"}";
 j+=",\"updates\":{\"ota_local_configured\":"+String(CIELO_ENABLE_OTA?"true":"false")+",\"ota_in_progress\":"+String(otaBusy?"true":"false");
 j+=",\"last_ota_message\":"+jsonQuote(lastOtaError);
 j+=",\"postboot_candidate_pending\":"+String(otaCandidateBoot?"true":"false");
 j+=",\"postboot_gatt_required\":"+String(otaCandidateNeedsGATT?"true":"false");
 j+=",\"postboot_gatt_pv\":"+String(cieloPvConfirmed>0?"true":"false");
 j+=",\"postboot_gatt_ch\":"+String(cieloChConfirmed>0?"true":"false");
 j+=",\"optical_verification\":\"NO_DISPONIBLE\",\"github_check\":\"NO_USADO\",\"cloud\":\"DESACTIVADA\"}";
 j+=",\"recovery\":{\"ap_ssid\":\"CIELO96-CONFIG\",\"ap_ip\":\"192.168.4.1\",\"password_location\":\"SERIAL_USB_LOCAL\",\"always_on\":true";
 j+=",\"running_partition\":"+jsonQuote(otaRunningPartition)+",\"next_update_partition\":"+jsonQuote(otaNextPartition);
 j+=",\"physical_ota_grant\":false,\"physical_ota_required\":false";
 j+=",\"ota_enabled\":"+String(CIELO_ENABLE_OTA?"true":"false")+"}";
 time_t currentEpoch=time(nullptr);
 struct tm clockLocal{};
 char clockOffset[8]="";
 if(timeOK()){
  localtime_r(&currentEpoch,&clockLocal);
  strftime(clockOffset,sizeof(clockOffset),"%z",&clockLocal);
 }
 j+=",\"recent_restarts\":"+restartHistoryJson();
 j+=",\"daily_monitoring\":"+cieloTrendJson();
 j+=",\"clock_diagnostics\":{\"epoch\":"+String((uint32_t)currentEpoch);
 j+=",\"valid\":"+String(timeOK()?"true":"false");
 j+=",\"timezone\":\"Europe/Madrid\",\"utc_offset\":"+jsonQuote(clockOffset);
 j+=",\"daylight_saving\":"+String(timeOK()&&clockLocal.tm_isdst>0?"true":"false");
 j+=",\"phone_sync_this_boot\":"+String(phoneSyncedThisBoot?"true":"false")+"}";
 j+=",\"lab\":"+(srv.hasArg("share")?String("null"):labJson());
 j+=",\"persistent_diagnostics\":{\"boot_history\":[";
 for(uint8_t i=0;i<bootHistoryCount;i++){
  uint8_t idx=(bootHistoryHead+8-bootHistoryCount+i)%8;
  if(i)j+=",";
  const auto &b=bootHistory[idx];
  j+="{\"boot_id\":"+String(b.id)+",\"reset_code\":"+String(b.cause);
  j+=",\"reset_name\":"+jsonQuote(resetReasonName((esp_reset_reason_t)b.cause));
  j+=",\"previous_clean\":"+String(b.previous_clean?"true":"false")+"}";
 }
 j+="],\"important_events\":[";
 for(uint8_t i=0;i<journal.count;i++){
  uint8_t idx=(journal.head+8-journal.count+i)%8;
  if(i)j+=",";
  const auto &e=journal.entries[idx];
  j+="{\"boot_id\":"+String(e.boot_id)+",\"utc_epoch\":"+(e.utc_epoch?String(e.utc_epoch):String("null"));
  j+=",\"event\":"+jsonQuote(e.code)+",\"repetitions\":"+String(e.repeats)+"}";
 }
 j+="],\"pending_nvs_write\":"+String(journalDirty?"true":"false");
 j+=",\"flash_policy\":\"Solo al arrancar y al registrar incidencias; maximo un lote cada 5 minutos\"}";
 j+=",\"event_retention\":\"36_RAM_8_incidencias_NVS_sin_detalles\",\"events\":[";
 for(int i=0;i<eventCount;i++){
  int idx=(eventHead-eventCount+i+EVENT_MAX)%EVENT_MAX;
  if(i)j+=',';
  j+="{\"t_ms\":"+String(events[idx].elapsed)+",\"boot_id\":"+String(previousBootCount+1);
  j+=",\"event\":"+jsonQuote(events[idx].label)+",\"detail\":"+(srv.hasArg("share")?String("null"):jsonQuote(events[idx].detail))+"}";
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
static void manualPv(){
 if(otaTxn.phase==OtaPhase::Preparing||otaTxn.phase==OtaPhase::Ready||
    otaTxn.phase==OtaPhase::Uploading)return fail(423,"OTA preparando o instalando");
 if(!labTxEnabled() || srv.arg("lab_manual")!="1")return fail(423,"Laboratorio: habilita 90 segundos de transmisiones manuales");
 if(srv.arg("mode")=="auto"){pezManual=false;lastPvDesired=false;submitPv(desiredPv());return ok();}
 if(!timeOK())return fail(503,"Reloj aun sin sincronizar");
 unsigned a[4]{};const char* k[]={"r","g","b","bright"};
 for(int i=0;i<4;i++){if(!positiveInt(srv.arg(k[i]),a[i])||a[i]>(i==3?100:255))return fail(400,"RGB o brillo incorrecto");}
 pezManual=true;pezManualUntil=millis()+20000UL;
 submitPv({uint8_t(a[0]),uint8_t(a[1]),uint8_t(a[2]),uint8_t(a[3])},20000);record("PV_MANUAL","20 segundos");ok();
}
static void manualCh(){
 if(otaTxn.phase==OtaPhase::Preparing||otaTxn.phase==OtaPhase::Ready||
    otaTxn.phase==OtaPhase::Uploading)return fail(423,"OTA preparando o instalando");
 if(!labTxEnabled() || srv.arg("lab_manual")!="1")return fail(423,"Laboratorio: habilita 90 segundos de transmisiones manuales");
 if(!timeOK())return fail(503,"Reloj aun sin sincronizar");
 unsigned a[3]{};const char* k[]={"r","g","b"};
 for(int i=0;i<3;i++){if(!positiveInt(srv.arg(k[i]),a[i])||a[i]>100)return fail(400,"Canal Chihiros incorrecto");}
 chiManual=true;chiManualUntil=millis()+8000UL;
 submitCh({uint8_t(a[0]),uint8_t(a[1]),uint8_t(a[2])},8000);record("CH_MANUAL","8 segundos");ok();
}
// Access is local and password-free, as requested by the owner.
// The rescue AP remains read-only; cross-origin web forms cannot issue commands.
static bool requireLocalRead(){
 if(!cielo96::security::localHost(srv.hostHeader().c_str(),WiFi.localIP().toString().c_str(),"192.168.4.1")){
  fail(403,"Utiliza la IP del ATOM o iwagumi-cielo96.local");return false;
 }
 return true;
}
static bool requirePrivateRead(){return requireLocalRead();}
static bool requireAdminWrite(){
 if(!requireLocalRead())return false;
 if(rescueOpen){fail(403,"Rescate es solo lectura");return false;}
 if(!cielo96::security::sameLanOrigin(srv.header("Origin").c_str(),srv.hostHeader().c_str()) ||
    !cielo96::security::safeFetchSite(srv.header("Sec-Fetch-Site").c_str())){
  fail(403,"Abre CIELO directamente desde su direccion local");return false;
 }
 return true;
}
static cielo96::monitor::Snapshot monitorSnapshot(){
 const uint32_t now=millis();
 cielo96::monitor::Snapshot m{};m.firmware=VERSION;m.bootId=previousBootCount+1;
 m.uptime=(now-bootMs)/1000UL;m.clockValid=timeOK();m.epoch=m.clockValid?(uint32_t)time(nullptr):0;
 m.heap=heap_caps_get_free_size(MALLOC_CAP_INTERNAL);m.heapMin=heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
 m.largestBlock=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
 m.bleReady=bleReady;m.workerStalled=bleReady&&cielo96::ble_worker_snapshot().heartbeat_age_ms>45000UL;
 m.ota=otaBusy||otaTxn.phase==OtaPhase::Preparing||otaTxn.phase==OtaPhase::Ready;
 m.offPending=pvOffObligation&&pvSafety!=PvSafetyState::OffGattOk;m.offFailures=pvOffFailures;
 m.communicationFreshSeconds=90;
 m.pv.expected=pvControlDesired||cieloPvRun||pvOffObligation;
 m.ch.expected=chReconnect.recovering||cieloChRun||cieloChReturnPending||cieloChAwaitResponse||cieloChWasDriven;
 m.chReturnFailed=cieloChWasDriven&&!cieloChRun&&!cieloChReturnPending&&!cieloChAwaitResponse&&cieloReturnTries>=2;
 m.pv.everOk=cieloLastSuccessfulPvAt>0;m.ch.everOk=cieloLastSuccessfulChAt>0;
 m.pv.okAgeSeconds=(now-cieloLastSuccessfulPvAt)/1000UL;m.ch.okAgeSeconds=(now-cieloLastSuccessfulChAt)/1000UL;
 m.pv.consecutiveErrors=cieloPvConsecutiveFailures;m.ch.consecutiveErrors=cieloChConsecutiveFailures;
 m.pv.operations=devices[0].completed;m.ch.operations=devices[1].completed;
 m.pv.errors=devices[0].errors;m.ch.errors=devices[1].errors;
 m.queueDepth=(queue.pending(cielo::ble::Device::PezVerde)?1:0)+(queue.pending(cielo::ble::Device::Chihiros)?1:0);
 m.workerQueueDepth=cielo96::ble_request_queue_depth();m.stackFreeBytes=cielo96::ble_stack_free_bytes();
 m.profileGeneration=profile.generation;m.pvPoints=profile.pvCount;m.chPoints=profile.chCount;
 m.bootMarkedHealthy=currentBootMarkedHealthy;m.resetReason=uint8_t(esp_reset_reason());
 m.recoveredBoot=bleRecoveryBoot;m.restartTotal=previousBootCount;
 m.lastStallPhase=bleRecoveryRtc.phase;m.lastStallDevice=bleRecoveryRtc.device;m.lastStallMs=bleRecoveryRtc.age_ms;
 m.recentCount=restartJournal.count;
 for(uint8_t i=0;i<m.recentCount;i++)m.recent[i]=cielo96::restarts::at(restartJournal,i);
 return m;
}
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
static bool otaNetworkReady(){
 return CIELO_ENABLE_OTA && !rescueOpen && WiFi.status()==WL_CONNECTED;
}
static bool otaBleHealthy(){
 if(!bleReady||cieloWatchdogTripped)return false;
 const auto snap=cielo96::ble_worker_snapshot();
 return snap.heartbeat_age_ms<15000UL &&
        (!snap.in_operation||snap.phase_age_ms<15000UL);
}
static String otaPreflightReason(){
 if(!otaBleHealthy())return "Bluetooth sin respuesta; revisa fase BLE del Informe Maestro";
 if(!currentBootMarkedHealthy){
  if(millis()-bootMs<180000UL)
   return String("Arranque en observacion: faltan ")+String((180000UL-(millis()-bootMs)+999UL)/1000UL)+" segundos";
  return "Arranque sin validacion: consulta el Informe Maestro";
 }
 if(!timeOK()||!cielo96::profileValid(profile))return "Hora o curvas no validas";
 if(labTxEnabled()||lab.scanPending||lab.probePending||queue.busy())
  return "Hay una operacion Bluetooth pendiente";
 if(cieloChReturnPending||cieloChAwaitResponse)return "Retorno de Chihiros pendiente";
 if(pvOffObligation && pvSafety!=PvSafetyState::OffGattOk)
  return "Pez Verde tiene POWER OFF nocturno pendiente; espera GATT o usa su app";
 return "";
}
static void otaPreflight(){
 if(rescueOpen)return fail(403,"Rescate solo lectura");
 const String reason=otaPreflightReason();
 String j="{\"ok\":true,\"ready\":";
 j+=reason.length()?"false":"true";
 j+=",\"reason\":"+jsonQuote(reason);
 j+=",\"ota_paired\":"+String("false");
 j+=",\"boot_observation_remaining_s\":"+String(!currentBootMarkedHealthy && millis()-bootMs<180000UL?(180000UL-(millis()-bootMs)+999UL)/1000UL:0);
 j+=",\"physical_permission\":false,\"physical_required\":false";
 j+=",\"physical_remaining_s\":"+String(0);
 j+=",\"pv_active\":"+String(cieloPvRun?"true":"false");
 j+=",\"ch_active\":"+String(cieloChRun?"true":"false");
 j+=",\"return_pending\":"+String(cieloChReturnPending?"true":"false");
 const auto bleSnap=cielo96::ble_worker_snapshot();
 j+=",\"ble_heartbeat_age_ms\":"+String(bleSnap.heartbeat_age_ms);
 j+=",\"ble_phase\":"+jsonQuote(cielo96::ble_worker_phase_name(bleSnap.phase));
 j+=",\"ble_phase_age_ms\":"+String(bleSnap.phase_age_ms);
 j+=",\"ble_device\":"+String(bleSnap.device);
 j+=",\"ota_boot_recovery\":\"app_rollback_when_setup_runs\"}";
 jsonSend(200,j);
}
static void otaResumePreviousControl(){
 if(!otaBleHealthy()||!timeOK()||!cielo96::profileValid(profile)||
    cieloChReturnPending||cieloChAwaitResponse||customResumePending){
  record("OTA_REANUDACION_MANUAL","No es seguro reanudar: verificar My Chihiros en su app");
  return;
 }
 cieloAlwaysOn=otaTxn.wasAlwaysOn;
 pvControlDesired=otaTxn.wasPv;
 cieloPvRun=otaTxn.wasPv;
 cieloChRun=otaTxn.wasCh;
 if(cieloChRun&&!prefs.putBool("v4_ch_takeover",true)){
  cieloChRun=false;
  record("OTA_REANUDACION_PARCIAL","NVS no disponible: Chihiros requiere My Chihiros");
 }
 if(cieloAlwaysOn) prefs.putBool("v5_always",true);
 lastPvDesired=false;lastChDesired=false;lastScheduleAt=millis()-30000UL;
 record("OTA_CONTROL_REANUDADO","Control previo solicitado (sin confirmacion optica)");
}
static void otaAbortPreparation(const char* reason){
 if(otaTxn.phase==OtaPhase::Uploading||otaTxn.phase==OtaPhase::Completed)return;
 if(otaTxn.phase==OtaPhase::Idle)return;
 otaTxn.phase=OtaPhase::Failed;
 otaTxn.reason=reason;
 record("OTA_PREPARACION_ABORTADA",otaTxn.reason);
 otaResumePreviousControl();
}
static void otaPreparationTick(){
 if(otaTxn.phase!=OtaPhase::Preparing&&otaTxn.phase!=OtaPhase::Ready)return;
 const uint32_t now=millis();
 if(now-otaTxn.started>90000UL){
  otaAbortPreparation("Tiempo de preparacion o permiso fisico agotado");return;
 }
 if(!otaBleHealthy()){
  otaAbortPreparation("Bluetooth dejo de responder; verificar My Chihiros");return;
 }
 if(otaTxn.phase==OtaPhase::Ready)return;
 if(queue.busy()||lab.scanPending||lab.probePending||customResumePending ||
    cieloChAwaitResponse||cieloChReturnPending)return;
 if(cieloChWasDriven){
  if(otaTxn.returnAsked){
   otaAbortPreparation("No se pudo confirmar el retorno GATT de My Chihiros");return;
  }
  otaTxn.returnAsked=true;
  cieloRequestReturn("Actualizacion OTA: ceder Chihiros a My Chihiros");
  otaTxn.reason="Devolviendo el control personalizado a My Chihiros";
  return;
 }
 if(!otaTxn.releaseAsked){
  if(!cielo96::ble_release_request(0))return;
  otaTxn.releaseAsked=true;
  otaTxn.reason="Liberando la conexion de Pez Verde";
  return;
 }
 if(!cielo96::ble_link_status(0).released_to_app)return;
 otaTxn.phase=OtaPhase::Ready;
 otaTxn.reason="Ambas conexiones cedidas; listo para recibir firmware";
 record("OTA_PREPARADA","Worker libre; Chihiros curva personalizada GATT OK, no verificacion optica");
}
static void otaPrepare(){
 if(!CIELO_ENABLE_OTA)return fail(503,"OTA desactivada");
 if(rescueOpen)return fail(403,"Rescate solo lectura");
 if(!requireAdminWrite())return;
 if(otaTxn.phase==OtaPhase::Preparing||otaTxn.phase==OtaPhase::Ready ||
    otaTxn.phase==OtaPhase::Uploading || otaBusy)
  return fail(409,"Ya existe una preparacion o carga OTA");
 if(!otaBleHealthy()||!currentBootMarkedHealthy||!timeOK()||
    !cielo96::profileValid(profile) || !wifiCoexistReady()||
    labTxEnabled()||lab.scanPending||lab.probePending||
    customResumePending||cieloChReturnPending||cieloChAwaitResponse)
  return fail(423,"No es seguro preparar OTA: consulta comprobacion previa");
 otaTxn=OtaTransaction{};
 otaTxn.phase=OtaPhase::Preparing;otaTxn.started=millis();
 otaTxn.wasPv=cieloPvRun;otaTxn.wasCh=cieloChRun;otaTxn.wasAlwaysOn=cieloAlwaysOn;
 otaTxn.reason="Deteniendo nuevas ordenes y vaciando cola Bluetooth";
 cieloPvRun=false;cieloChRun=false;
 queue.cancel_pending(cielo::ble::Device::PezVerde);
 queue.cancel_pending(cielo::ble::Device::Chihiros);
 record("OTA_PREPARACION","Control detenido temporalmente; se conserva perfil Natural V4");
 ok();
}
static void otaPrepareStatus(){
 String j="{\"phase\":";
 const char*phase="idle";
 switch(otaTxn.phase){
  case OtaPhase::Preparing:phase="preparing";break;
  case OtaPhase::Ready:phase="ready";break;
  case OtaPhase::Uploading:phase="uploading";break;
  case OtaPhase::Completed:phase="completed";break;
  case OtaPhase::Failed:phase="failed";break;
  default:break;
 }
 j+=jsonQuote(phase)+",\"reason\":"+jsonQuote(otaTxn.reason);
 j+=",\"age_ms\":"+String(otaTxn.started?millis()-otaTxn.started:0);
 j+=",\"ble_ok\":"+String(otaBleHealthy()?"true":"false");
 j+=",\"return_pending\":"+String(cieloChReturnPending||cieloChAwaitResponse?"true":"false")+"}";
 jsonSend(200,j);
}
static void otaPrepareCancel(){
 if(!requireAdminWrite())return;
 if(otaTxn.phase==OtaPhase::Uploading||otaBusy||otaTxn.phase==OtaPhase::Completed)
  return fail(409,"La instalacion ya esta en curso o completada");
 if(otaTxn.phase==OtaPhase::Preparing||otaTxn.phase==OtaPhase::Ready)
  otaAbortPreparation("Cancelada desde iPhone");
 else if(otaTxn.phase==OtaPhase::Failed)otaResumePreviousControl();
 ok();
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
 if(!otaAllowed)return fail(403,lastOtaError.c_str());
 if(otaError)return fail(500,lastOtaError.c_str());
 if(!otaSuccess)return fail(500,"La imagen no se ha completado");
 srv.send(200,"text/plain","Firmware recibido y validado por Update; reiniciando");
 delay(400);ESP.restart();
}
static void otaUpload(){
 HTTPUpload&upload=srv.upload();
 if(upload.status==UPLOAD_FILE_START){
  otaAllowed=otaTxn.phase==OtaPhase::Ready && requireAdminWrite() &&
             otaNetworkReady() && currentBootMarkedHealthy &&
             otaBleHealthy() && !labTxEnabled() && !queue.busy() && !otaBusy &&
             !lab.scanPending && !lab.probePending && !customResumePending &&
             !cieloPvRun && !cieloChRun && !cieloChReturnPending && !cieloChAwaitResponse;
  otaSuccess=false;otaError=false;otaBytes=0;otaDigestStop();
  if(!otaAllowed){
   if(otaTxn.phase!=OtaPhase::Ready)lastOtaError="Prepara las pantallas antes de cargar el BIN";
   else if(!otaNetworkReady())lastOtaError="Red Wi-Fi no disponible";
   else if(!currentBootMarkedHealthy)lastOtaError="Arranque aun no verificado";
   else if(!otaBleHealthy())
     lastOtaError="Bluetooth bloqueado: no se puede actualizar de forma segura";
   else if(cieloChReturnPending||cieloChAwaitResponse)
     lastOtaError="Retorno a My Chihiros pendiente: actualizacion detenida";
   else if(cieloPvRun||cieloChRun)
     lastOtaError="Control de luces activo: prepara la actualizacion antes de cargar el BIN";
   else if(queue.busy()||labTxEnabled())
     lastOtaError="Operacion Bluetooth en curso: espera antes de actualizar";
   else lastOtaError="Actualizacion no disponible en este momento";
   otaError=true;record("OTA_RECHAZADA",lastOtaError);return;
  }
  const String expected=srv.header("X-Cielo-Sha256");
  String filename=upload.filename;filename.toLowerCase();
  if(!hashLooksValid(expected) || !filename.endsWith(".bin")){
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
  otaBusy=true;otaLastChunkAt=millis();otaTxn.phase=OtaPhase::Uploading;
  otaTxn.reason="Recibiendo firmware y comprobando integridad SHA-256";
  record("OTA_START","BIN local sin clave; comprobacion de compatibilidad e integridad");
 }else if(upload.status==UPLOAD_FILE_WRITE){
  if(!otaAllowed||otaError)return;
  const esp_partition_t* target=esp_ota_get_next_update_partition(nullptr);
  bool written=target && upload.currentSize<=target->size && otaBytes<=target->size-upload.currentSize;
  size_t consumed=0;
  if(written && otaBytes<cielo96::ota::prefixBytes){
   const size_t needed=cielo96::ota::prefixBytes-otaBytes;
   consumed=upload.currentSize<needed?upload.currentSize:needed;
   memcpy(otaPrefix+otaBytes,upload.buf,consumed);
   if(otaBytes+consumed==cielo96::ota::prefixBytes){
    written=cielo96::ota::compatiblePrefix(otaPrefix,sizeof(otaPrefix)) &&
     Update.write(otaPrefix,sizeof(otaPrefix))==sizeof(otaPrefix);
   }
  }
  if(written && consumed<upload.currentSize)
   written=Update.write(upload.buf+consumed,upload.currentSize-consumed)==upload.currentSize-consumed;
  if(!written || mbedtls_sha256_update_ret(&otaDigest,upload.buf,upload.currentSize)!=0){
   lastOtaError="BIN incompatible o escritura OTA interrumpida";otaError=true;Update.abort();otaDigestStop();
  }else {otaBytes+=upload.currentSize;otaLastChunkAt=millis();}
 }else if(upload.status==UPLOAD_FILE_END){
  if(!otaAllowed||otaError){
   otaBusy=false;otaDigestStop();
   if(otaTxn.phase==OtaPhase::Uploading) {
    otaTxn.phase=OtaPhase::Ready;
    otaAbortPreparation(lastOtaError.c_str());
   }
   return;
  }
  if(!otaDigestStarted || mbedtls_sha256_finish_ret(&otaDigest,otaDigestOut)!=0){
   otaError=true;lastOtaError="SHA256 no disponible";Update.abort();otaDigestStop();otaBusy=false;return;
  }
  otaDigestStop();
  static const char hex[]="0123456789abcdef";
  char actual[65];for(unsigned i=0;i<32;i++){actual[i*2]=hex[otaDigestOut[i]>>4];actual[i*2+1]=hex[otaDigestOut[i]&15];}
  actual[64]='\0';
  if(strcasecmp(actual,otaExpectedSha)!=0 || otaBytes<500000){
   otaError=true;lastOtaError="Hash o tamano de firmware no coinciden";Update.abort();
  }else if(prefs.putString("ota_prev",otaRunningPartition)!=otaRunningPartition.length() ||
           prefs.putUChar("ota_attempts",0)!=1){
   lastOtaError="No se pudo guardar el retorno OTA en NVS: instalacion cancelada";
   otaError=true;Update.abort();
  }else if(!Update.end(true)){
   lastOtaError="BIN no validado como firmware ESP32-S3";
   otaError=true;Update.abort();prefs.remove("ota_prev");
  }else{
   otaSuccess=true;otaTxn.phase=OtaPhase::Completed;
   otaTxn.reason="Firmware recibido y verificado por Update; reinicio pendiente";
   lastOtaError="SHA256 coincide, cabecera ESP32-S3/8MB y Update aceptan el BIN";
   otaPhysicalUntil=0;
   record("OTA_OK",String("bytes=")+String(otaBytes)+" validado");
  }
  otaBusy=false;
  if(otaError&&otaTxn.phase==OtaPhase::Uploading){
   otaTxn.phase=OtaPhase::Ready;otaAbortPreparation(lastOtaError.c_str());
  }
 }else if(upload.status==UPLOAD_FILE_ABORTED){
  otaError=true;otaBusy=false;otaAllowed=false;otaSuccess=false;lastOtaError="Carga OTA interrumpida; no se instalo firmware";Update.abort();otaDigestStop();
  if(otaTxn.phase==OtaPhase::Uploading){
   otaTxn.phase=OtaPhase::Ready;otaAbortPreparation("Carga OTA interrumpida");
  }
 }
}
static void configureWifi(){
 String ssid=srv.arg("ssid"),pass=srv.arg("password");
 for(size_t i=0;i<ssid.length();i++){
  const unsigned char c=(unsigned char)ssid[i];
  if(c<32||c==127)return fail(400,"Nombre Wi-Fi contiene caracteres de control");
 }
 if(ssid.length()<1||ssid.length()>32||pass.length()<8||pass.length()>63)
  return fail(400,"SSID o clave WPA2 fuera de rango");
 if(prefs.putString("ssid",ssid)!=ssid.length() ||
    prefs.putString("wifi_pw",pass)!=pass.length())
  return fail(507,"No se pudo guardar la Wi-Fi completa");
 storedSSID=ssid;storedPass=pass;
 record("WIFI_NUEVA","Cambio solicitado; la clave no se expone en informes");
 lastWiFiAttempt=millis()-31000UL;
 WiFi.disconnect(false,false);
 ok();
}
static void configureRoutes(){
 cielo96::monitor::attach(srv,prefs,requireAdminWrite,requirePrivateRead,monitorSnapshot);
 srv.on("/api/maintenance",HTTP_GET,[]{if(requirePrivateRead())jsonSend(200,cielo96::maintenanceInventory());});
  // Recursos PWA de solo lectura. No habilitar cache de comandos u OTA offline.
  srv.on("/manifest.webmanifest",HTTP_GET,[]{srv.sendHeader("Cache-Control","no-store");srv.send_P(200,"application/manifest+json",CIELO_MANIFEST);});
  srv.on("/icon.svg",HTTP_GET,[]{srv.sendHeader("Cache-Control","public, max-age=86400");srv.send_P(200,"image/svg+xml",CIELO_ICON_SVG);});
  srv.on("/icon-180.png",HTTP_GET,[]{srv.sendHeader("Cache-Control","public, max-age=86400");srv.send_P(200,"image/png",reinterpret_cast<const char*>(CIELO_ICON_180_PNG),sizeof(CIELO_ICON_180_PNG));});

 const char*keys[]={"Origin","Sec-Fetch-Site","X-Cielo-Monitor","X-Cielo-Sha256"};srv.collectHeaders(keys,sizeof(keys)/sizeof(keys[0]));
 srv.on("/",HTTP_GET,[]{if(!requireLocalRead())return;++httpCount;srv.sendHeader("Cache-Control","no-store");srv.sendHeader("X-Frame-Options","DENY");srv.send_P(200,"text/html; charset=utf-8",CIELO_UI);});
 srv.on("/api/auth/status",HTTP_GET,[]{jsonSend(200,"{\"authenticated\":true,\"mode\":\"LAN_NO_PASSWORD\"}");});
 srv.on("/api/status",HTTP_GET,[]{if(requireLocalRead())statusJson();});
 srv.on("/api/diagnostico/maestro",HTTP_GET,[]{if(!requireLocalRead())return;if(!srv.hasArg("share")&&!requirePrivateRead())return;if(srv.hasArg("download"))srv.sendHeader("Content-Disposition",srv.hasArg("share")?"attachment; filename=\"CIELO_INFORME_COMPARTIBLE.json\"":"attachment; filename=\"CIELO_INFORME_MAESTRO.json\"");diagnosticJson();});
 srv.on("/api/service/report",HTTP_GET,[]{if(requireLocalRead()&&(srv.hasArg("share")||requirePrivateRead()))diagnosticJson();});
 srv.on("/api/lab/status",HTTP_GET,[]{if(requirePrivateRead())jsonSend(200,labJson());});
 srv.on("/api/lab/start",HTTP_POST,[]{if(!requireAdminWrite())return;
  if(rescueOpen)return fail(403,"Rescate es solo lectura");
  if(lab.scanPending)return fail(409,"Termina primero el escaneo BLE");
  labStart();ok();
 });
 srv.on("/api/lab/stop",HTTP_POST,[]{if(!requireAdminWrite())return;
  if(rescueOpen)return fail(403,"Rescate es solo lectura");
  lab.running=false;lab.txArmed=false;record("LAB_PARADA","Muestreo y transmision manual detenidos");ok();
 });
 srv.on("/api/lab/scan",HTTP_POST,[]{if(!requireAdminWrite())return;
  if(otaTxn.phase==OtaPhase::Preparing||otaTxn.phase==OtaPhase::Ready||
     otaTxn.phase==OtaPhase::Uploading)return fail(423,"OTA bloquea nuevos escaneos BLE");
  if(rescueOpen)return fail(403,"Rescate es solo lectura");
  if(!otaBleHealthy())return fail(423,"Bluetooth no responde: no iniciar escaneo");
  if(lab.scanPending||lab.probePending)return fail(409,"Ya hay otra prueba BLE en marcha");
  if(!cielo96::ble_scan_request()){lab.scanRejects++;return fail(503,"Cola de escaneos no disponible");}
  lab.scanRequests++;lab.scanPending=true;lab.scanStartedMs=millis();record("LAB_SCAN_START","Escaneo BLE activo 5s; sin conectar ni escribir GATT");ok();
 });
 srv.on("/api/lab/probe",HTTP_POST,[]{if(!requireAdminWrite())return;
  if(otaTxn.phase==OtaPhase::Preparing||otaTxn.phase==OtaPhase::Ready||
     otaTxn.phase==OtaPhase::Uploading)return fail(423,"OTA bloquea sondas BLE");
  if(rescueOpen)return fail(403,"Rescate es solo lectura");
  if(!otaBleHealthy())return fail(423,"Bluetooth no responde: no iniciar sonda");
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
 srv.on("/api/cielo/v4/status",HTTP_GET,[]{if(requireLocalRead())jsonSend(200,cieloModeJson());});
 srv.on("/api/cielo/v4/start",HTTP_POST,[]{if(!requireAdminWrite())return;
  if(otaTxn.phase==OtaPhase::Preparing||otaTxn.phase==OtaPhase::Ready||
     otaTxn.phase==OtaPhase::Uploading)return fail(423,"OTA preparando o instalando");
  if(rescueOpen)return fail(403,"Rescate de solo lectura");
  if(srv.arg("confirm")!="ACTIVAR_NATURAL_V4")return fail(403,"Confirma el control de ambas luces");
  if(!currentBootMarkedHealthy || !bleReady || !timeOK() ||
     !wifiCoexistReady() || !cielo96::profileValid(profile))
   return fail(423,"Espera arranque estable (3 min), BLE, reloj y perfil validos");
  if(queue.busy()||customResumePending||cieloChReturnPending||
     cieloChAwaitResponse||lab.scanPending||lab.probePending||lab.txArmed||
     contrastStage==1||contrastStage==2||contrastStage==4)
   return fail(409,"Existe otra operacion o prueba: termina y repite");
  if(!prefs.putBool("v4_ch_takeover",true))
   return fail(507,"No se pudo proteger el retorno de My Chihiros en NVS");
  if(!prefs.putBool("v5_always",true))
   return fail(507,"No se pudo guardar el control continuo en NVS");
  if(!prefs.putBool("pv_auto",true))
   return fail(507,"No se pudo conservar la recuperacion independiente de Pez Verde");
  cieloAlwaysOn=true;cieloResumePending=false;chReconnect.cancel();
  cieloPvRun=true;cieloChRun=true;cieloWatchdogTripped=false;
  pvControlDesired=true;pvSafety=PvSafetyState::Active;pvRetryNext=0;pvOffNextTry=0;
  cieloPvConsecutiveFailures=0;cieloChConsecutiveFailures=0;
  cieloStartedAt=millis();lastPvDesired=false;lastChDesired=false;
  lastScheduleAt=millis()-30000UL;
  pezManual=false;chiManual=false;
  record("V5_24H_ACTIVADA",String("Curvas activas 24h PV=")+String(profile.pvCount)+" CH="+String(profile.chCount)+"; apagado fisico via POWER OFF");
  ok();
 });
 srv.on("/api/cielo/v4/return",HTTP_POST,[]{if(!requireAdminWrite())return;
  if(otaTxn.phase==OtaPhase::Preparing||otaTxn.phase==OtaPhase::Ready||
     otaTxn.phase==OtaPhase::Uploading)return fail(423,"OTA preparando o instalando");
  if(rescueOpen)return fail(403,"Rescate de solo lectura");
  if(srv.arg("confirm")!="VOLVER_MY_CHIHIROS")return fail(403,"Confirma el retorno");
  if(!bleReady)return fail(423,"BLE no disponible: abre My Chihiros directamente");
  if(customResumePending||lab.scanPending||lab.probePending||
     contrastStage==1||contrastStage==2||contrastStage==4)
   return fail(409,"Espera a que finalice el ensayo previo");
  cieloChRun=false;cieloAlwaysOn=false;cieloResumePending=false;chReconnect.cancel();prefs.putBool("v5_always",false);
  cieloRequestReturn("Emergencia: volver a My Chihiros");
  record("V4_BOTON_MYCHIHIROS","Pez Verde sigue usando Natural V4 si estaba activa");
  ok();
 });
 srv.on("/api/cielo/v4/pause",HTTP_POST,[]{if(!requireAdminWrite())return;
  if(otaTxn.phase==OtaPhase::Preparing||otaTxn.phase==OtaPhase::Ready||
     otaTxn.phase==OtaPhase::Uploading)return fail(423,"OTA ya gestiona las luces: no pulses pausa");
  if(rescueOpen)return fail(403,"Rescate de solo lectura");
  if(srv.arg("confirm")!="PAUSAR_TODO")return fail(403,"Confirma pausa de ambas pantallas");
  cieloPvRun=false;cieloChRun=false;cieloAlwaysOn=false;cieloResumePending=false;chReconnect.cancel();prefs.putBool("v5_always",false);
  pvControlDesired=false;pvOffObligation=false;pvOffInFlightSeq=0;pvSafety=PvSafetyState::Paused;
  if(!prefs.putBool("pv_auto",false))record("PV_NVS_FALLO","Pausa PV no persistida, verificar tras reinicio");
  queue.cancel_pending(cielo::ble::Device::PezVerde);
  if(bleReady)cieloRequestReturn("Pausa total: devolver Chihiros a My Chihiros");
  else {queue.cancel_pending(cielo::ble::Device::Chihiros);record("V4_SIN_BLE","No se puede restaurar Chihiros sin BLE; requiere My Chihiros");}
  record("V4_PAUSA_TOTAL","Pez Verde queda en su ultimo estado; sin nuevos envios de CIELO");
  ok();
 });
 srv.on("/api/chihiros/contrast/start",HTTP_POST,[]{if(!requireAdminWrite())return;
  if(rescueOpen)return fail(403,"Rescate solo lectura");
  if(cieloPvRun||cieloChRun)return fail(409,"Pausa Natural V4 antes de ejecutar contrastes");
  if(srv.arg("confirm")!="ENSAYO_30_30_30")return fail(403,"Confirmacion necesaria");
  if(!bleReady||!wifiCoexistReady()||!timeOK())return fail(423,"Bluetooth o reloj sin preparar");
  if(queue.busy()||customResumePending||lab.scanPending||lab.probePending||
     contrastStage==1||contrastStage==4)return fail(409,"Espera a que termine el enlace anterior");
  lab.txArmed=true;lab.txUntil=millis()+LAB_TX_WINDOW_MS;lab.txArmCount++;
  if(!queue.submit(cielo::ble::Device::Chihiros,cielo::ble::Action::Apply,
                   30,30,30,0,millis(),45000)){
   lab.txArmed=false;return fail(503,"No se pudo enviar el contraste 30");
  }
  chiCieloNight=false;chiManual=true;chiManualUntil=millis()+120000UL;
  contrastStage=1;contrastObserved=false;contrastStartedAt=millis();contrastManualAt=0;
  record("CH_CONTRASTE_INICIO","Se solicita 30/30/30 y se espera confirmacion visual del usuario");
  ok();
 });
 srv.on("/api/chihiros/contrast/cancel",HTTP_POST,[]{if(!requireAdminWrite())return;
  if(rescueOpen)return fail(403,"Rescate solo lectura");
  if(queue.busy()||contrastStage==1||contrastStage==4)
   return fail(409,"Espera a que termine el envío Bluetooth antes de cancelar");
  lab.txArmed=false;chiManual=false;
  if(contrastStage==1||contrastStage==2)contrastStage=7;
  if(!queue.busy())cielo96::ble_release_request(1);
  record("CH_CONTRASTE_ABORTADO","Ensayo cancelado: recuperar desde My Chihiros si es necesario");
  ok();
 });
 srv.on("/api/chihiros/my-curve-test",HTTP_POST,[]{if(!requireAdminWrite())return;
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
 srv.on("/api/ble/release",HTTP_POST,[]{if(!requireAdminWrite())return;
  if(rescueOpen)return fail(403,"Rescate solo lectura");
  if(!bleReady)return fail(423,"Bluetooth aun no inicializado");
  if(queue.busy()||lab.probePending||lab.scanPending)return fail(409,"Espera a que termine la operacion Bluetooth");
  const String device=srv.arg("device");
  if(device!="chihiros"&&device!="pez_verde")return fail(400,"Elige una pantalla valida");
  const uint8_t index=device=="chihiros"?1:0;
  if(device=="chihiros" && cieloChRun)
   return fail(409,"Para volver a My Chihiros utiliza el boton Curvas: Volver a My Chihiros");
  if(!cielo96::ble_release_request(index))return fail(503,"No se pudo solicitar la liberacion");
  record("BLE_CEDER",device+" liberacion solicitada sin ordenes RGB");
  ok();
 });
 srv.on("/api/lab/preview",HTTP_POST,[]{if(!requireAdminWrite())return;
  if(rescueOpen)return fail(403,"Rescate solo lectura");
  if(cieloPvRun||cieloChRun)return fail(409,"Pausa Natural V4 antes del modo laboratorio");
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
 srv.on("/api/lab/tx/arm",HTTP_POST,[]{if(!requireAdminWrite())return;
  if(rescueOpen)return fail(403,"Rescate es solo lectura");
  if(cieloPvRun||cieloChRun)return fail(409,"Primero pausa el modo Natural V4");
  if(srv.arg("confirm")!="HABILITAR_MANUAL")return fail(403,"Confirmacion explicita requerida");
  if(!bleReady||!wifiCoexistReady())return fail(423,"Bluetooth o coexistencia no preparados");
  lab.txArmed=true;lab.txUntil=millis()+LAB_TX_WINDOW_MS;lab.txArmCount++;
  record("LAB_TX_ARMADA","Solo ordenes manuales 90 segundos; scheduler desconectado");ok();
 });
 srv.on("/api/lab/tx/disarm",HTTP_POST,[]{if(!requireAdminWrite())return;
  lab.txArmed=false;record("LAB_TX_DESARMADA","Transmision manual bloqueada");ok();
 });
 srv.on("/api/lab/browser-sample",HTTP_POST,[]{if(!requireAdminWrite())return;
  if(rescueOpen)return fail(403,"Rescate es solo lectura");
  unsigned count=0,avg=0,maximum=0;
  if(!parseBoundedUInt(srv.arg("count"),count,40)||count<1||
     !parseBoundedUInt(srv.arg("avg"),avg,60000)||
     !parseBoundedUInt(srv.arg("max"),maximum,60000))return fail(400,"Muestra de latencia incorrecta");
  lab.browserSamples+=count;lab.browserAvgMs=avg;
  if(maximum>lab.browserMaxMs)lab.browserMaxMs=maximum;
  record("LAB_HTTP","Latencia desde navegador media "+String(avg)+" ms max "+String(maximum)+" ms");ok();
 });
 srv.on("/api/service/status",HTTP_GET,[]{jsonSend(200,String("{\"version\":")+jsonQuote(VERSION)+",\"accessMode\":\"LAN_NO_PASSWORD\",\"otaAuthRequired\":false,\"otaEnabled\":"+String(CIELO_ENABLE_OTA?"true":"false")+",\"otaPaired\":"+String("false")+",\"otaGrantSeconds\":"+String(0)+",\"otaBusy\":"+String(otaBusy?"true":"false")+",\"otaBytes\":"+String(otaBytes)+"}");});
 srv.on("/api/clock",HTTP_POST,[]{if(!requireAdminWrite())return;if(rescueOpen)return fail(403,"Rescate solo lectura");unsigned epoch;if(!positiveInt(srv.arg("epoch"),epoch)){
   String s=srv.arg("epoch");if(s.length()<10||s.length()>11)return fail(400,"Epoch incorrecto");
   for(unsigned i=0;i<s.length();i++)if(s[i]<'0'||s[i]>'9')return fail(400,"Epoch incorrecto");
   unsigned long long val=strtoull(s.c_str(),nullptr,10);if(val<1700000000ULL||val>4102444800ULL)return fail(400,"Epoch fuera de rango");
   struct timeval tv{(time_t)val,0};if(settimeofday(&tv,nullptr)!=0)return fail(500,"No se pudo fijar el reloj");phoneSyncedThisBoot=true;record("HORA_SYNC","Reloj desde telefono");return ok();
  }return fail(400,"Epoch demasiado corto");});
  srv.on("/api/curves/commit",HTTP_POST,[]{if(!requireAdminWrite())return;
   if(rescueOpen||otaBusy||otaTxn.phase!=OtaPhase::Idle)return fail(423,"Cambios bloqueados durante rescate o OTA");
   saveBothCurves();
  });
  srv.on("/api/cielo/cadence",HTTP_POST,[]{if(!requireAdminWrite())return;
   if(rescueOpen)return fail(423,"Rescate de solo lectura");cadenceSet();
  });
  srv.on("/api/cielo/cadence",HTTP_GET,[]{if(requirePrivateRead())jsonSend(200,cadenceJson());});
 srv.on("/api/save",HTTP_POST,[]{if(!requireAdminWrite())return;if(rescueOpen)return fail(403,"Rescate solo lectura");return fail(410,"Ruta antigua retirada: guarda ambas luces con /api/curves/commit");});
 srv.on("/api/reset",HTTP_POST,[]{if(!requireAdminWrite())return;if(rescueOpen)return fail(403,"Rescate solo lectura");return fail(410,"Ruta antigua retirada: guarda ambas luces con /api/curves/commit");});
 srv.on("/api/chihiros/curve",HTTP_POST,[]{if(!requireAdminWrite())return;if(rescueOpen)return fail(403,"Rescate solo lectura");return fail(410,"Ruta antigua retirada: guarda ambas luces con /api/curves/commit");});
 srv.on("/api/chihiros/curve/reset",HTTP_POST,[]{if(!requireAdminWrite())return;if(rescueOpen)return fail(403,"Rescate solo lectura");return fail(410,"Ruta antigua retirada: guarda ambas luces con /api/curves/commit");});
 srv.on("/api/manual",HTTP_POST,[]{if(!requireAdminWrite())return;if(rescueOpen)return fail(403,"Rescate solo lectura");manualPv();});
 srv.on("/api/chihiros/manual",HTTP_POST,[]{if(!requireAdminWrite())return;if(rescueOpen)return fail(403,"Rescate solo lectura");manualCh();});
 srv.on("/api/chihiros/stop",HTTP_POST,[]{if(!requireAdminWrite())return;
  record("CH_AUTO_INSEGURO","Solicitud rechazado: 0x12 activa escenas y produjo apagado fisico");
  return fail(423,"AUTO Chihiros bloqueado: el modo 0x12 apaga DYSL60; restaura desde My Chihiros");
 });
 srv.on("/api/chihiros/evening/off",HTTP_POST,[]{if(!requireAdminWrite())return;if(rescueOpen)return fail(403,"Rescate solo lectura");return fail(423,"Programador detenido en R4.4; usa Laboratorio para AUTO manual");});
 srv.on("/api/chihiros/evening/on",HTTP_POST,[]{if(!requireAdminWrite())return;if(rescueOpen)return fail(403,"Rescate solo lectura");return fail(423,"Programador detenido en R4.4; activacion pendiente de pruebas opticas");});
 srv.on("/api/chihiros/live/stop",HTTP_POST,[]{if(!requireAdminWrite())return;if(rescueOpen)return fail(403,"Rescate solo lectura");return fail(423,"Usa Laboratorio con autorizacion para AUTO manual");});
 srv.on("/api/wifi/scan",HTTP_POST,[]{if(!requireAdminWrite())return;
  if(rescueOpen)return fail(403,"Rescate solo lectura");
  if(queue.busy()||otaBusy)return fail(409,"Espera al final de la transmision o actualizacion");
  WiFi.scanDelete();
  const int result=WiFi.scanNetworks(true,false);
  if(result==-2)return fail(503,"No se pudo iniciar la busqueda Wi-Fi");
  record("WIFI_BUSQUEDA","Escaneo asincrono a peticion del usuario, no modifica red actual");
  ok();
 });
 srv.on("/api/wifi/networks",HTTP_GET,[]{if(!requirePrivateRead())return;
  const int count=WiFi.scanComplete();
  String j="{\"pending\":"+String(count==-1?"true":"false")+",\"ssid\":"+jsonQuote(WiFi.SSID());
  j+=",\"ip\":"+jsonQuote(WiFi.status()==WL_CONNECTED?WiFi.localIP().toString():"");
  j+=",\"recovery_ap\":\"CIELO96-CONFIG\",\"recovery_ip\":\"192.168.4.1\",\"networks\":[";
  if(count>0){
   const int limit=count>16?16:count;
   for(int n=0;n<limit;n++){
    if(n)j+=",";
    j+="{\"ssid\":"+jsonQuote(WiFi.SSID(n))+",\"rssi\":"+String(WiFi.RSSI(n))+"}";
   }
  }
  j+="]}";jsonSend(200,j);
 });
 srv.on("/api/wifi",HTTP_POST,[]{if(!requireAdminWrite())return;if(rescueOpen)return fail(403,"Rescate solo lectura");configureWifi();});
 srv.on("/api/wifi/forget",HTTP_POST,[]{if(!requireAdminWrite())return;if(rescueOpen)return fail(403,"Rescate solo lectura");prefs.remove("ssid");prefs.remove("wifi_pw");storedSSID="";storedPass="";WiFi.disconnect(false,false);ok();});
 srv.on("/api/ota/preflight",HTTP_GET,[]{if(requirePrivateRead())otaPreflight();});
 srv.on("/api/ota/prepare",HTTP_POST,otaPrepare);
 srv.on("/api/ota/prepare/status",HTTP_GET,[]{if(requirePrivateRead())otaPrepareStatus();});
 srv.on("/api/ota/prepare/cancel",HTTP_POST,otaPrepareCancel);
 srv.on("/api/ota/pair",HTTP_POST,[]{if(!requireAdminWrite())return;ok();}); // legacy clients: no key needed
 srv.on("/api/ota/upload",HTTP_POST,[]{
  if(rescueOpen)return fail(403,"Rescate solo lectura");
  startOtaResponse();
 },otaUpload);
 const char*unsupported[]={"/api/chihiros/test","/api/storm/start","/api/storm/stop",
   "/api/chihiros/aurora931/apply","/api/chihiros/aurora931/restore",
   "/api/pez/cielo931/apply","/api/pez/cielo931/restore","/api/pez/cielo94/apply","/api/pez/cielo94/restore",
   "/api/updates/check","/api/updates/install","/api/updates/status",
   "/api/diagnostico/nube/configurar","/api/diagnostico/nube/desactivar","/api/diagnostico/nube/estado","/api/diagnostico/nube/sincronizar"};
 for(const char*path:unsupported){srv.on(path,HTTP_ANY,[]{fail(410,"Funcion antigua retirada; no se ha modificado la iluminacion");});}
 srv.onNotFound([](){fail(404,"Ruta no disponible en CIELO");});
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
  if(!mdnsReady && now-mdnsLastAttempt>30000UL){
   mdnsLastAttempt=now;
   mdnsReady=MDNS.begin("iwagumi-cielo96");
   if(mdnsReady){MDNS.addService("http","tcp",80);record("MDNS_INICIADO","http://iwagumi-cielo96.local");}
   else record("MDNS_PENDIENTE","Se reintentara cuando la red este preparada");
  }
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
static void cieloRequestReturn(const char* reason){
 queue.cancel_pending(cielo::ble::Device::Chihiros);
 lastChDesired=false;chiManual=false;chiCieloNight=false;
 if(cieloChReturnPending||cieloChAwaitResponse)return;
 cieloReturnTries=0;cieloReturnRetryAt=millis();
 cieloChReturnPending=true;cieloReturnReason=reason;
 record("V4_RETORNO_PEDIDO",cieloReturnReason+"; selector personalizado 0x03, no escenas 0x12");
}
static void lightingTask(){
 const uint32_t now=millis();
 if(cieloWatchdogTripped && bleReady && !queue.busy() &&
    cielo96::ble_worker_snapshot().heartbeat_age_ms<3000UL){
  cieloWatchdogTripped=false;pvRetryNext=now;lastPvDesired=false;
  if(pvControlDesired)pvSafety=PvSafetyState::Degraded;
  record("BLE_WORKER_RECUPERADO","Mismo trabajador BLE activo; reintentar seguridad nocturna");
 }
 if(pezManual && (int32_t)(now-pezManualUntil)>=0)pezManual=false;
 if(chiManual && (int32_t)(now-chiManualUntil)>=0)chiManual=false;
 if(!cieloPvRun && !cieloChRun && !cieloChWasDriven && !pvOffObligation && !pvControlDesired)return;
 if(bleReady && !cieloWatchdogTripped &&
    cielo96::ble_worker_snapshot().heartbeat_age_ms>45000UL){
  cieloWatchdogTripped=true;cieloPvRun=false;cieloChRun=false;
  if(cieloAlwaysOn)chReconnect.arm(now);
  if(pvControlDesired){pvSafety=PvSafetyState::Degraded;pvRetryNext=now+5000UL;}
  queue.cancel_pending(cielo::ble::Device::PezVerde);
  if(cieloChWasDriven)cieloRequestReturn("Watchdog BLE 45s: parada preventiva y retorno pendiente");
  cieloFallbacks++;
  record("V4_WATCHDOG_PAUSA","BLE no responde: no se garantiza retorno hasta recuperar radio");
  return;
 }
 if(pvControlDesired && timeOK() && cielo96::profileValid(profile)){
  const bool night=cielo96::safety::offRequired(pvControlDesired,timeOK(),cielo96::profileValid(profile),desiredPv().brightness);
  if(night && !pvOffObligation){
   pvOffObligation=true;pvOffNextTry=now;pvSafety=PvSafetyState::OffPending;
   queue.cancel_pending(cielo::ble::Device::PezVerde);lastPvDesired=false;
   record("PV_OFF_OBLIGATORIO","Consigna cero: POWER OFF prioritario; RGB anterior cancelado");
  }else if(!night && pvOffObligation && !bleRecoveryBootPvSafe){
   pvOffObligation=false;pvOffNextTry=0;pvOffInFlightSeq=0;pvSafety=PvSafetyState::Active;
   queue.cancel_pending(cielo::ble::Device::PezVerde);lastPvDesired=false;
   record("PV_DIA_REANUDADO","Curva actual recuperada; ningun RGB obsoleto");
  }
 }
  if(cadenceTrial15){
   const auto worker=cielo96::ble_worker_snapshot();
   const bool safe=cielo96::curves::secondsModeSafe(wifiCoexistReady(),
     bleReady&&!cieloWatchdogTripped&&worker.heartbeat_age_ms<15000UL,
     otaTxn.phase==OtaPhase::Idle&&!otaBusy,
     queue.busy()&&worker.phase_age_ms>11000UL,
     heap_caps_get_free_size(MALLOC_CAP_INTERNAL),bleResultErrors-cadenceStartBleErrors);
   if((int32_t)(now-cadenceEndsAt)>=0||!safe){
    cadenceTrial15=false;cadenceFallbacks++;
    cadenceFallbackReason=(int32_t)(now-cadenceEndsAt)>=0?"Ensayo 20 min terminado":"BLE, WiFi, OTA, cola o heap no seguros";
    lastPvDesired=false;lastChDesired=false;
    record("CADENCIA_RETORNO_30S",cadenceFallbackReason);
   }
  }
  const uint32_t cadenceMs=activeCadenceMs();
  if(now-lastScheduleAt<cadenceMs && !pvOffObligation)return;
  const bool cadenceDue=now-lastScheduleAt>=cadenceMs;
  if(cadenceDue){lastScheduleAt=now;
    const auto phase=cielo96::ble_worker_snapshot();
    cadenceDiagnostics.sample(queue.busy(),queue.pending(cielo::ble::Device::PezVerde)||queue.pending(cielo::ble::Device::Chihiros),phase.phase_age_ms);
    if(pvOffObligation)cadenceDiagnostics.offPriority++;

    if(cadenceTrial15)cadenceTicks15++;else cadenceTicks30++;
    if(queue.busy()||queue.pending(cielo::ble::Device::PezVerde)||queue.pending(cielo::ble::Device::Chihiros))cadenceSkippedBusy++;
  }
 if(!timeOK() || !cielo96::profileValid(profile)){
  if(cadenceDue)cadenceDiagnostics.invalidClock++;
  if(cieloChWasDriven && !cieloChReturnPending && !cieloChAwaitResponse){
   cieloChRun=false;cieloPvRun=false;cieloFallbacks++;
   cieloRequestReturn("Hora o curva invalida: ceder a My Chihiros");
  }
  if(!bleRecoveryBootPvSafe)return;
 }
 if(!bleReady || !wifiCoexistReady() || rescueOpen){if(cadenceDue)cadenceDiagnostics.inhibited++;return;}
 if(cadenceDue)cadenceDiagnostics.evaluated++;
 const bool bleAlive=!cieloWatchdogTripped && (cielo96::ble_worker_snapshot().heartbeat_age_ms<15000UL);
 if(pvOffObligation && pvControlDesired && !otaBusy &&
    otaTxn.phase!=OtaPhase::Preparing && otaTxn.phase!=OtaPhase::Ready &&
    otaTxn.phase!=OtaPhase::Uploading && bleAlive &&
    cielo96::safety::retryDue(now,pvOffNextTry) && !queue.busy() &&
    !customResumePending && !cieloChAwaitResponse){
  queue.cancel_pending(cielo::ble::Device::PezVerde);
  if(queue.submit(cielo::ble::Device::PezVerde,cielo::ble::Action::Apply,0,0,0,0,now,90000UL)){
   pvOffScheduled++;
   pvSafety=pvOffFailures?PvSafetyState::OffRetrying:PvSafetyState::OffPending;
   pvOffNextTry=now+60000UL;
   record("PV_POWER_OFF_ENCOLADO",String("Programado ")+String(pvOffScheduled)+"; envio GATT pendiente");
  }else pvOffNextTry=now+5000UL;
 }
 if(pvControlDesired && !pvOffObligation && !cieloPvRun && bleAlive &&
    cielo96::safety::retryDue(now,pvRetryNext) && otaTxn.phase==OtaPhase::Idle){
  cieloPvRun=true;pvSafety=PvSafetyState::Degraded;lastPvDesired=false;
  record("PV_RECONECTANDO","RGB actual; no reproducir consignas obsoletas");
 }
 const int minute=minuteOfDay();
 const bool chWindow=true; // CIELO controla 24h, incluido RGB 0/0/0
 if(cieloPvRun && !pvOffObligation && !pezManual && !lab.txArmed && !queue.pending(cielo::ble::Device::PezVerde)){
  const auto v=desiredPv();
  const bool changed=!lastPvDesired||memcmp(&v,&prevPv,sizeof(v))!=0;
  const bool heartbeat=cielo96::presence::refreshDue(now,lastPvDesiredAt); // comprobar BLE aun con RGB estable
  if(changed||heartbeat){
   if(queue.submit(cielo::ble::Device::PezVerde,cielo::ble::Action::Apply,
                   v.r,v.g,v.b,v.brightness,now,90000UL)){
    prevPv=v;lastPvDesired=true;lastPvDesiredAt=now;cieloPvOrders++;
   }
  }
 }
 if(cieloChRun && !chSelectiveHold && chWindow && !chiManual && !lab.txArmed &&
    !cieloChReturnPending && !cieloChAwaitResponse && !customResumePending &&
    !queue.pending(cielo::ble::Device::Chihiros)){
  const auto v=desiredCh();
  const bool changed=!lastChDesired||memcmp(&v,&prevCh,sizeof(v))!=0;
  const bool heartbeat=cielo96::presence::refreshDue(now,lastChDesiredAt);
  if(changed||heartbeat){
   bool markerReady=cieloChWasDriven||prefs.getBool("v4_ch_takeover",false);
   if(!markerReady)markerReady=prefs.putBool("v4_ch_takeover",true)!=0;
   if(!markerReady){
    cieloChRun=false;cieloFallbacks++;
    cieloRequestReturn("NVS no disponible: controlar desde My Chihiros");
   }else if(queue.submit(cielo::ble::Device::Chihiros,cielo::ble::Action::Apply,
                   v.r,v.g,v.b,0,now,90000UL)){
    prevCh=v;lastChDesired=true;lastChDesiredAt=now;cieloChOrders++;
   }
  }
 } // Sin cambio automatico a My Chihiros por horario.
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
  if(res.action==static_cast<uint8_t>(cielo::ble::Action::Apply) &&
     (cieloPvRun||cieloChRun||cieloChWasDriven||pvOffObligation)){
   if(res.device==0 && (cieloPvRun||pvOffObligation)){
    const bool wasOffCommand=pvOffObligation && (res.off_sent||cielo96::safety::isOffAck(pvOffObligation,pvOffInFlightSeq,res.sequence));
    if(wasOffCommand)pvOffInFlightSeq=0;
    if(pvOffObligation && !wasOffCommand){
     record("PV_RESULTADO_ANTERIOR","Un resultado GATT previo no confirma apagado nocturno");
    }else if(res.success){
     cieloPvConfirmed++;cieloPvConsecutiveFailures=0;cieloLastSuccessfulPvAt=now;
     if(wasOffCommand){
      pvOffLastGatt=now;pvOffFailures=0;pvSafety=PvSafetyState::OffGattOk;
      pvOffNextTry=now+60000UL;
      record("PV_POWER_OFF_GATT_OK","POWER OFF GATT OK; sin verificacion optica");
     }else {
      pvSafety=PvSafetyState::Active;
      if(pvDayRecoveryPending){
       pvDayRecoveryPending=false;pvDayRecoveryGattOk++;
       record("PV_DIA_RECUPERACION_GATT_OK","Consigna actual aceptada por GATT; luminosidad real no verificada");
      }
     }
    }else if(wasOffCommand){
     ++pvOffFailures;
     const uint32_t backoff=cielo96::safety::offRetryDelayMs(pvOffFailures);
     pvOffNextTry=now+backoff;pvSafety=PvSafetyState::OffRetrying;
     record("PV_POWER_OFF_REINTENTO",String("Fallo ")+String(pvOffFailures)+"; cola BLE reintenta; siguiente programacion de seguridad en "+String(backoff/1000UL)+"s");
    }else if(++cieloPvConsecutiveFailures>=3){
     cieloPvRun=false;queue.cancel_pending(cielo::ble::Device::PezVerde);
     pvSafety=PvSafetyState::Degraded;pvRetryNext=now+30000UL;
     cieloFallbacks++;record("PV_DEGRADADA","3 fallos BLE; recuperacion automatica y apagado nocturno prioritario");
    }
   }
   if(res.device==1 && (cieloChRun||cieloChWasDriven)){
    if(res.success){
     cieloChConfirmed++;cieloChWasDriven=true;cieloChConsecutiveFailures=0;cieloLastSuccessfulChAt=now;chReconnect.success();
    }else if(++cieloChConsecutiveFailures>=2){
     cieloChRun=false;cieloFallbacks++;
     if(cieloAlwaysOn)chReconnect.arm(now);
     cieloRequestReturn("2 errores BLE de Chihiros; recuperacion de My Chihiros");
    }
   }
  }
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
 if(timeoutNow>bleSoftTimeouts){bleSoftTimeouts=timeoutNow;record("BLE_SOFT_TIMEOUT","Operacion supera 3,5s; puede terminar correctamente. No es por si sola un fallo");}
 const bool pvDayFirst=pvDayRecoveryPending && cielo96::pvday::defer_ch_return(true,now-pvDayRecoveryStarted);
 const bool pvOffFirst=pvOffObligation && pvSafety!=PvSafetyState::OffGattOk &&
   pvOffFailures<3 && queue.pending(cielo::ble::Device::PezVerde);
 if(cieloChReturnPending && !chSelectiveHold && !pvOffFirst && !pvDayFirst && !queue.busy() && !customResumePending &&
    !lab.scanPending && !lab.probePending &&
    (int32_t)(now-cieloReturnRetryAt)>=0){
   if(cielo96::ble_custom_resume_request()){
    cieloChReturnPending=false;cieloChAwaitResponse=true;
    customResumePending=true;customResumeRequested=now;cieloReturnTries++;
    record("V4_RETORNO_ENVIANDO",String("Intento ")+String(cieloReturnTries)+" modo personalizado 0x03");
   }else{
    cieloReturnRetryAt=now+2500UL;
   }
 }
 if((cieloChReturnPending && !chSelectiveHold && !pvOffFirst && !pvDayFirst)||cieloChAwaitResponse||customResumePending||
    lab.scanPending||lab.probePending)return;
 cielo::ble::Command cmd{};
 if(queue.start_next(now,cmd,(pvOffObligation && pvSafety!=PvSafetyState::OffGattOk)||pvDayFirst)){
  const bool schedulerPV=(cieloPvRun||pvOffObligation)&&cmd.device==cielo::ble::Device::PezVerde&&cmd.action==cielo::ble::Action::Apply;
  if(cieloChReturnPending && (pvOffFirst||pvDayFirst) && cmd.device!=cielo::ble::Device::PezVerde){
   queue.complete(cmd.sequence,false,now);
   record("PV_OFF_PRIORIDAD","La orden POWER OFF de Pez Verde precede al retorno My Chihiros");
   return;
  }
  if(schedulerPV && cielo96::safety::forbidStaleRgb(pvOffObligation,cmd.brightness)){
   queue.complete(cmd.sequence,false,now);
   queue.cancel_pending(cielo::ble::Device::PezVerde);
   record("PV_RGB_OBSOLETO_BLOQUEADO","No enviar RGB viejo durante obligacion de apagado");
   return;
  }
  if(schedulerPV && pvOffObligation && cmd.brightness==0){
   pvOffInFlightSeq=cmd.sequence;pvOffAttempts++;pvOffLastTry=now;
   record("PV_POWER_OFF_INTENTO",String("Envio BLE intentado ")+String(pvOffAttempts)+"; resultado pendiente");
  }
  const bool schedulerCH=cieloChRun&&timeOK()&&
    cmd.device==cielo::ble::Device::Chihiros&&cmd.action==cielo::ble::Action::Apply;
  if(chSelectiveHold && cmd.device==cielo::ble::Device::Chihiros){
    queue.complete(cmd.sequence,false,now);
    record("CH_AISLAMIENTO_TX","Orden Chihiros no enviada: limpieza SDK pendiente");
  }else if(!labTxEnabled()&&!schedulerPV&&!schedulerCH){
    if(cmd.device==cielo::ble::Device::Chihiros && cmd.action==cielo::ble::Action::RestoreAuto){
     chiAutoPending=false;record("CH_AUTO_NO_ENVIADO","AUTO bloqueado; accion fisica pendiente");
    }
    queue.complete(cmd.sequence,false,now);
    lab.txBlocked++;record("LAB_TX_BLOQUEADA","Cola BLE abortada: autorizacion caducada");
  }else if(!cielo96::ble_dispatch(cmd)){
    queue.complete(cmd.sequence,false,now);
    if(pvOffObligation && pvOffInFlightSeq==cmd.sequence){
     pvOffInFlightSeq=0;pvOffFailures++;
     pvOffNextTry=now+cielo96::safety::offRetryDelayMs(pvOffFailures);
     pvSafety=PvSafetyState::OffRetrying;
     record("PV_POWER_OFF_REINTENTO","Cola BLE rechazada: apagado sigue pendiente");
    }
    record("BLE_COLA_LLENA","No se envio comando al worker");
  }
 }
}
static void chReconnectTick(){
 if(!cieloAlwaysOn){chReconnect.cancel();return;}
 const bool allowed=bleReady && !rescueOpen && !bleRecoveryBootPvSafe &&
  currentBootMarkedHealthy && timeOK() && cielo96::profileValid(profile) &&
  otaBleHealthy() && !otaBusy && otaTxn.phase==OtaPhase::Idle &&
  !labTxEnabled() && !lab.scanPending && !lab.probePending &&
  !chSelectiveHold && !customResumePending && !cieloChReturnPending && !cieloChAwaitResponse &&
  !queue.busy() && !queue.pending(cielo::ble::Device::Chihiros);
 if(!chReconnect.ready(millis(),allowed))return;
 if(prefs.putBool("v4_ch_takeover",true)!=1){chReconnect.defer(millis());return;}
 chReconnect.begin();cieloChRun=true;cieloChConsecutiveFailures=0;
 lastChDesired=false;lastScheduleAt=millis()-30000UL;
 record("CH_RECONEXION_AUTO","Recuperar consigna actual; sin reproducir colores anteriores");
}
static void bleRecoveryTick(){
 const auto snapshot=cielo96::ble_recovery_snapshot();
 const auto actual=cielo96::ble_blockage_snapshot();
 const bool inhibited=otaBusy || rescueOpen || otaTxn.phase==OtaPhase::Preparing ||
  otaTxn.phase==OtaPhase::Ready || otaTxn.phase==OtaPhase::Uploading || otaTxn.phase==OtaPhase::Completed;
 const bool selective=cielo96::selective::hold_ch_only({
  bleReady,actual.pending_ch_age_ms>0,actual.pending_pv_age_ms>0,
  actual.pending_ch_age_ms,actual.worker.heartbeat_age_ms,
  actual.worker.phase_age_ms,actual.worker.device,
  pvOffObligation,inhibited});
 if(selective!=chSelectiveHold){
  chSelectiveHold=selective;
  if(selective){
   ++chSelectiveEpisodes;
   queue.cancel_pending(cielo::ble::Device::Chihiros);
   record("CH_AISLAMIENTO_INICIO","SDK Chihiros pendiente; worker vivo; PV sigue independiente; sin liberar cliente inseguro");
  }else{
   lastChDesired=false;
   record("CH_AISLAMIENTO_FIN","Se reevalua Chihiros; si sigue pendiente o vence plazo, rescate global sigue disponible");
  }
 }
 // Isolated CH cleanup does not imply a dead worker.
 // PV safety and 300 s global fallback are unchanged.
 const uint32_t hbAge=selective?actual.worker.heartbeat_age_ms:snapshot.heartbeat_age_ms;
 const uint32_t phaseAge=selective?actual.worker.phase_age_ms:snapshot.phase_age_ms;
 const auto decision=bleSupervisor.tick(millis(),bleReady,hbAge,
  phaseAge,inhibited,bleRecoveryRtc);
 if(decision==cielo96::recovery::Decision::Exhausted){
  if(!bleRecoveryExhausted)record("BLE_RESCATE_AGOTADO","2 reinicios sin 30min estables: acceso web conservado; revisar luces");
  bleRecoveryExhausted=true;return;
 }
 if(snapshot.heartbeat_age_ms<15000UL)bleRecoveryExhausted=false;
 if(decision!=cielo96::recovery::Decision::Restart)return;
 // No vTaskDelete, BLEDevice::deinit ni reset de colas con una llamada pendiente.
 // Reiniciar el MCU entero elimina conjuntamente tareas, callbacks y radio.
 cielo96::recovery::armRestart(bleRecoveryRtc,snapshot.phase,snapshot.device,snapshot.phase_age_ms);
 bleRecoveryRtc.epoch=timeOK()?uint32_t(time(nullptr)):0;
 bleRecoveryRtc.originBoot=previousBootCount+1;cielo96::recovery::seal(bleRecoveryRtc);
 const auto diagnosticSnapshot=cielo96::ble_blockage_snapshot();
 const auto diagnosis=cielo96::blockage::classify({
  diagnosticSnapshot.worker.phase,diagnosticSnapshot.worker.device,
  diagnosticSnapshot.worker.heartbeat_age_ms,diagnosticSnapshot.worker.phase_age_ms,
  diagnosticSnapshot.pending_pv_age_ms,diagnosticSnapshot.pending_ch_age_ms});
 record("BLE_REINICIO_DIAGNOSTICO",String(cielo96::blockage::name(diagnosis.primary))+
  ", sdk_pending="+String(diagnosis.sdk_pending?"si":"no")+
  ", pending_device="+String(diagnosis.pending_device)+
  ", pending_age_ms="+String(diagnosis.pending_age_ms)+
  ", worker_heartbeat_ms="+String(diagnosticSnapshot.worker.heartbeat_age_ms)+
  ", worker_phase="+String(cielo96::ble_worker_phase_name(diagnosticSnapshot.worker.phase)));
 record("BLE_REINICIO_CONTROL",String("Fase ")+cielo96::ble_worker_phase_name(snapshot.phase)+
  ", dispositivo "+String(snapshot.device)+", bloqueo "+String(snapshot.phase_age_ms)+"ms");
 // RTC conserva la evidencia aunque la cuota del journal NVS impida escribir ahora.
 delay(50);ESP.restart();
}
void setup(){
 bootMs=millis();Serial.begin(115200);delay(100);
 if(esp_reset_reason()!=ESP_RST_SW || !cielo96::recovery::valid(bleRecoveryRtc)){
  bleRecoveryRtc=cielo96::recovery::Retained{};cielo96::recovery::seal(bleRecoveryRtc);
 }
 bleRecoveryBoot=bleRecoveryRtc.pending==1;
 bleRecoveryRtc.pending=0;cielo96::recovery::seal(bleRecoveryRtc);
 uint32_t previousPhaseCode=rtcTrace.phase;
 lastResetTraceValid=(rtcTrace.magic==RTC_TRACE_MAGIC &&
  rtcTrace.check==(RTC_TRACE_MAGIC ^ RTC_TRACE_SALT ^ previousPhaseCode));
 if(lastResetTraceValid)lastResetTracePhase=previousPhaseCode;
 tracePhase(TracePhase::Setup);
 Serial.printf("IWAGUMI CIELO %s | ATOMS3-Lite | build experimental\n",VERSION);
 tracePhase(TracePhase::Nvs);
 prefs.begin("cielo96",false);
 previousBootCount=prefs.getUInt("boots",0);
 if(prefs.getBytesLength("restart_v2")==sizeof(restartJournal)){
  prefs.getBytes("restart_v2",&restartJournal,sizeof(restartJournal));
  if(!cielo96::restarts::valid(restartJournal))restartJournal=cielo96::restarts::Journal{};
 }
 cielo96::restarts::Entry entry{};entry.boot=previousBootCount+1;entry.previous=previousBootCount;
 entry.reset=uint8_t(esp_reset_reason());entry.device=255;
 if(bleRecoveryBoot){entry.bleRecovery=true;entry.phase=bleRecoveryRtc.phase;
  entry.device=bleRecoveryRtc.device;entry.stallMs=bleRecoveryRtc.age_ms;entry.stallEpoch=bleRecoveryRtc.epoch;}
 cielo96::restarts::append(restartJournal,entry);
 restartJournalDirty=prefs.putBytes("restart_v2",&restartJournal,sizeof(restartJournal))!=sizeof(restartJournal);
 restartJournalSavedAt=millis();
 priorEvent=prefs.getString("fault_type","ninguno");
 priorEventDetail=prefs.getString("fault_text","ninguno");
 savedFailures=prefs.getUInt("fault_count",0);
 priorBootClean=prefs.getBool("clean",false);
 if(prefs.getBytesLength("boot_hist")==sizeof(bootHistory)){
  prefs.getBytes("boot_hist",bootHistory,sizeof(bootHistory));
  for(const auto &e:bootHistory)if(e.id)bootHistoryCount++;
  if(bootHistoryCount>8)bootHistoryCount=8;
  uint32_t newest=0;uint8_t newestIndex=7;
  for(uint8_t i=0;i<8;i++)if(bootHistory[i].id>=newest){newest=bootHistory[i].id;newestIndex=i;}
  bootHistoryHead=(newestIndex+1)%8;
 }
 bootHistory[bootHistoryHead]={previousBootCount+1,(uint8_t)esp_reset_reason(),(uint8_t)priorBootClean,0};
 bootHistoryHead=(bootHistoryHead+1)%8;if(bootHistoryCount<8)bootHistoryCount++;
 prefs.putBytes("boot_hist",bootHistory,sizeof(bootHistory));
 if(prefs.getBytesLength("diag_journal")==sizeof(journal)){
  prefs.getBytes("diag_journal",&journal,sizeof(journal));
  if(journal.magic!=JOURNAL_MAGIC||journal.count>8||journal.head>=8)journal=JournalStore{JOURNAL_MAGIC,0,0,0,{}};
 }

 prefs.putUInt("boots",previousBootCount+1);
 prefs.putBool("clean",false);
 const esp_partition_t *running=esp_ota_get_running_partition();
 const String previousOtaApp=prefs.getString("ota_prev","");
 otaCandidateBoot=running && (previousOtaApp=="app0"||previousOtaApp=="app1") &&
    previousOtaApp!=String(running->label);
 otaCandidateNeedsGATT=otaCandidateBoot && cielo96::autonomy::enabled(prefs.isKey("v5_always"),prefs.getBool("v5_always",false));
 if(otaCandidateBoot){
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
 loadProfile();
 cieloAlwaysOn=cielo96::autonomy::enabled(prefs.isKey("v5_always"),prefs.getBool("v5_always",false));
 if(cieloAlwaysOn && !prefs.isKey("v5_always") && prefs.putBool("v5_always",true)!=1){
  cieloAlwaysOn=false;record("AUTO_NVS_FALLO","No se pudo guardar el arranque automatico");
 }
 pvControlDesired=prefs.getBool("pv_auto",cieloAlwaysOn);
 if(pvControlDesired && !prefs.isKey("pv_auto") && prefs.putBool("pv_auto",true)!=1){
  pvControlDesired=false;cieloAlwaysOn=false;record("AUTO_NVS_FALLO","No se pudo guardar control PV");
 }
 cieloResumePending=cieloAlwaysOn;
 if(bleRecoveryBoot){
  record("BLE_RESCATE_ARRANQUE",String("Reinicio automatico ")+String(bleRecoveryRtc.attempts)+"; sin confirmacion optica");
  if(pvControlDesired){
   bleRecoveryBootPvSafe=true;pvOffObligation=true;pvOffNextTry=millis();pvSafety=PvSafetyState::OffPending;
  }
 }
 if(pvControlDesired){
  bleRecoveryBootPvSafe=true;pvOffObligation=true;pvOffNextTry=millis();pvSafety=PvSafetyState::OffPending;
 }
 if(prefs.getBool("v4_ch_takeover",false)){
  cieloChWasDriven=true;cieloChReturnPending=true;
  cieloReturnRetryAt=millis()+30000UL;
  cieloReturnReason="Recuperacion preventiva tras reinicio del ATOM";
  record("V4_RECUPERACION_BOOT",cieloReturnReason);
 }
 storedSSID=prefs.getString("ssid",CIELO_WIFI_SSID);storedPass=prefs.getString("wifi_pw",CIELO_WIFI_PASSWORD);
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
 Serial.printf("CIELO LAN: HTTP sin clave; loop=%u bytes, minimo libre=%u bytes; heap=%u, mayor=%u\n",
  (unsigned)CIELO_LOOP_STACK_BYTES,(unsigned)uxTaskGetStackHighWaterMark(nullptr),
  (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
 record("LAN_INICIADA","HTTP local sin contrasena; HTTPS no iniciado para reservar RAM a BLE");
 record("WEB_PRIMERO","HTTP y AP de recuperación inicializados; Bluetooth en espera");
 tracePhase(TracePhase::Idle);
}
void loop(){
 if(!currentBootMarkedHealthy && millis()-bootMs>=180000UL){
  const bool baseHealthy=!otaCandidateBoot ||
   (WiFi.status()==WL_CONNECTED && wifiCoexistReady() &&
    otaBleHealthy() && timeOK() && cielo96::profileValid(profile) &&
    !cieloWatchdogTripped && !cieloChReturnPending &&
    !cieloChAwaitResponse && !customResumePending);
  if(baseHealthy){
   currentBootMarkedHealthy=true;
   prefs.putBool("clean",true);
   record("ARRANQUE_BASE_VALIDADO",otaCandidateBoot?
     "OTA: HTTP, WiFi, reloj, perfil y BLE listos; pendiente verificacion de GATT":
     "Arranque estable 180 segundos");
  }
 }
 if(otaCandidateBoot && currentBootMarkedHealthy){
  const bool gattReady=(!otaCandidateNeedsGATT ||
   (cieloPvConfirmed>0 && cieloChConfirmed>0)) &&
   WiFi.status()==WL_CONNECTED && wifiCoexistReady() && otaBleHealthy() &&
   timeOK() && cielo96::profileValid(profile) && !cieloWatchdogTripped &&
   !cieloChReturnPending && !cieloChAwaitResponse && !customResumePending && !otaBusy;
  if(otaHealthyWindow.tick(millis(),gattReady)){
   if(prefs.putUChar("ota_attempts",0)==1 && prefs.remove("ota_prev")){
    otaCandidateBoot=false;
    record("OTA_POSTARRANQUE_OK","WiFi+reloj+BLE+perfil y GATT cuando procede; optica no medida");
   }
  }
 }
 if(otaCandidateBoot && millis()-bootMs>=600000UL){
  const String back=prefs.getString("ota_prev","");
  const esp_partition_subtype_t subtype=back=="app0"?
   ESP_PARTITION_SUBTYPE_APP_OTA_0:ESP_PARTITION_SUBTYPE_APP_OTA_1;
  const esp_partition_t*safe=(back=="app0"||back=="app1")?
   esp_partition_find_first(ESP_PARTITION_TYPE_APP,subtype,nullptr):nullptr;
  if(safe && esp_ota_set_boot_partition(safe)==ESP_OK){
   record("OTA_ROLLBACK","Sin salud completa tras 10min; regreso a particion anterior");
   delay(150);ESP.restart();
  }else{
   record("OTA_ROLLBACK_IMPOSIBLE","Recuperar manualmente por USB");
   otaCandidateBoot=false;
  }
 }
 if(otaBusy && millis()-otaLastChunkAt>120000UL){
  Update.abort();otaBusy=false;otaAllowed=false;otaError=true;
  lastOtaError="OTA detenida por falta de datos durante 120 segundos";
  otaDigestStop();
  if(otaTxn.phase==OtaPhase::Uploading){
   otaTxn.phase=OtaPhase::Ready;otaAbortPreparation(lastOtaError.c_str());
  }
 }
 tracePhase(TracePhase::Http);srv.handleClient();
 tracePhase(TracePhase::Button);physicalButtonTask();
 tracePhase(TracePhase::WifiTick);wifiTask();
 if(!blePilotRequested && !blePilotAttempted && !bleReady && !rescueOpen &&
    millis()-bootMs>=30000UL && WiFi.status()==WL_CONNECTED && wifiCoexistReady()){
  blePilotRequested=true;
  record("BLE_AUTO_SOLICITADO","WiFi listo, 30s desde arranque; solo inicializar radio");
 }
 if(blePilotRequested && !blePilotAttempted && !rescueOpen){
  tracePhase(TracePhase::Ble);
  blePilotRequested=false;
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
 // The worker checks this atomic interlock again after connecting and before each PV frame.
 cielo96::ble_set_pv_off_required(pvOffObligation);
 cielo96::ble_keep_links(!otaBusy&&otaTxn.phase==OtaPhase::Idle&&!rescueOpen&&pvControlDesired,
  !otaBusy&&otaTxn.phase==OtaPhase::Idle&&!rescueOpen&&cieloChRun&&!cieloChReturnPending);
 // Tras un rescate, encolar OFF antes que el retorno Chihiros, incluso sin hora.
 if(bleRecoveryBootPvSafe)lightingTask();
 if(bleReady && !rescueOpen){tracePhase(TracePhase::Ble);bleTick();}
 if(bleRecoveryBootPvSafe && pvOffLastGatt && currentBootMarkedHealthy && timeOK() &&
    cielo96::profileValid(profile) && desiredPv().brightness==0 && !cieloWatchdogTripped){
  bleRecoveryBootPvSafe=false;
  record("BLE_RESCATE_PV_OFF_OK","OFF GATT completado; reanudar consigna actual, optica pendiente");
 }
 // Fast daytime PV restoration AFTER OFF GATT and valid current curve.
 // CH return is deferred only briefly, and PV is never allowed to bypass night OFF.
 if(pvControlDesired && cielo96::pvday::can_resume({
    bleRecoveryBootPvSafe,pvOffLastGatt!=0,currentBootMarkedHealthy,
    timeOK(),cielo96::profileValid(profile),bleReady,
    otaBusy||rescueOpen||otaTxn.phase!=OtaPhase::Idle,
    cielo96::ble_worker_snapshot().heartbeat_age_ms<15000UL,
    uint8_t(timeOK()&&cielo96::profileValid(profile)?desiredPv().brightness:0)})){
   bleRecoveryBootPvSafe=false;
   pvOffObligation=false;pvOffNextTry=0;pvOffInFlightSeq=0;
   cielo96::ble_set_pv_off_required(false);
   queue.cancel_pending(cielo::ble::Device::PezVerde);
   queue.cancel_pending(cielo::ble::Device::Chihiros);
   cieloPvRun=true;lastPvDesired=false;lastScheduleAt=millis()-30000UL;
   pvDayRecoveryPending=true;pvDayRecoveryStarted=millis();pvDayRecoveryAttempts++;
   pvSafety=PvSafetyState::Degraded;
   record("PV_DIA_RECUPERACION_RAPIDA","OFF GATT previo; programar brillo actual sin esperar retorno CH; no hay verificacion optica");
 }
 if(pvDayRecoveryPending && !cielo96::pvday::defer_ch_return(true,millis()-pvDayRecoveryStarted)){
  pvDayRecoveryPending=false;
  record("PV_DIA_RECUPERACION_LIMITE","20s sin GATT confirmado; liberar retorno CH y mantener reintentos PV");
 }
 if(cieloResumePending && cieloAlwaysOn && !bleRecoveryBootPvSafe && currentBootMarkedHealthy &&
    bleReady && timeOK() && wifiCoexistReady() && !rescueOpen &&
    cielo96::profileValid(profile) && !customResumePending &&
    !cieloChReturnPending && !cieloChAwaitResponse && !queue.busy()){
  cieloResumePending=false;cieloPvRun=true;cieloChRun=true;
  pvControlDesired=true;
  cieloWatchdogTripped=false;cieloPvConsecutiveFailures=0;cieloChConsecutiveFailures=0;
  lastPvDesired=false;lastChDesired=false;
  cieloStartedAt=millis();lastScheduleAt=millis()-30000UL;
  record("V5_AUTO_REANUDADO","Ambas curvas reanudadas tras comprobar hora, WiFi y Bluetooth");
 }
 lightingTask();
 cielo96::ble_set_pv_off_required(pvOffObligation);
 cieloTrendTick();
 labTick();
 chReconnectTick();
 otaPreparationTick();
 bleRecoveryTick();
 if(!bootEpoch&&timeOK()&&restartJournal.count){
  bootEpoch=uint32_t(time(nullptr))-(millis()-bootMs)/1000UL;
  auto&e=restartJournal.entries[(restartJournal.head+7)%8];e.epoch=bootEpoch;
  cielo96::restarts::seal(restartJournal);restartJournalDirty=true;
 }
 if(currentBootMarkedHealthy&&bleReady&&!cieloWatchdogTripped&&
    (!pvControlDesired||cieloLastSuccessfulPvAt>0)&&(!cieloAlwaysOn||cieloLastSuccessfulChAt>0)){
  if(cielo96::restarts::recovered(restartJournal,previousBootCount+1,timeOK()?uint32_t(time(nullptr)):0))restartJournalDirty=true;
 }
 flushRestartJournal();
 flushJournal(); // nunca escribir en flash por polling de diagnóstico
 tracePhase(TracePhase::Led);statusLedTick();
 tracePhase(TracePhase::Idle);delay(3);
}
