"""Ejecuta el supervisor real de main.cpp con reinicio y reloj simulados."""
from pathlib import Path
import subprocess,tempfile
s=Path('src/main.cpp').read_text();function=s[s.index('static void bleRecoveryTick(){'):s.index('void setup(){')]
fixture=r'''
#include "cielo_ble_recovery.h"
#include "cielo_ble_blockage.h"
#include "cielo_ble_selective.h"
#include <cassert>
#include <string>
using std::string;
string String(const char* v){return v;}
string String(unsigned v){return std::to_string(v);}
unsigned millis(){return 200000;}
void delay(unsigned){}
void record(const char*,const string&){}
struct Esp {unsigned calls=0;void restart(){calls++;}} ESP;
namespace cielo96 {
struct Snapshot{unsigned heartbeat_age_ms=1359609,phase_age_ms=1358846;unsigned char phase=4,device=0;} sample;
Snapshot ble_recovery_snapshot(){return sample;}
struct BleBlockageSnapshot {Snapshot worker;unsigned pending_pv_age_ms=0,pending_ch_age_ms=0;};
BleBlockageSnapshot ble_blockage_snapshot(){return {sample,0,0};}
const char* ble_worker_phase_name(unsigned char){return "CONEXION";}
}
enum class OtaPhase {Idle,Preparing,Ready,Uploading,Completed,Failed};
struct {OtaPhase phase=OtaPhase::Idle;} otaTxn;
bool timeOK(){return true;}
unsigned long time(void*){return 1792750000;}
unsigned previousBootCount=111;
bool otaBusy=false,rescueOpen=false,bleReady=true,bleRecoveryExhausted=false;
bool pvOffObligation=false,chSelectiveHold=false,lastChDesired=false;
unsigned chSelectiveEpisodes=0;
namespace cielo {namespace ble {enum class Device {Chihiros};}}
struct QueueStub {void cancel_pending(cielo::ble::Device){}} queue;
void saveBleRtcPhaseTrace(unsigned){}
cielo96::recovery::Retained bleRecoveryRtc{};
cielo96::recovery::Supervisor bleSupervisor;
'''
main=r'''
int main(){
 cielo96::recovery::seal(bleRecoveryRtc);
 for(auto p:{OtaPhase::Preparing,OtaPhase::Ready,OtaPhase::Uploading,OtaPhase::Completed}){
  otaTxn.phase=p;bleRecoveryTick();assert(ESP.calls==0);
 }
 otaTxn.phase=OtaPhase::Idle;otaBusy=true;bleRecoveryTick();assert(ESP.calls==0);
 otaBusy=false;rescueOpen=true;bleRecoveryTick();assert(ESP.calls==0);
 rescueOpen=false;bleRecoveryTick();assert(ESP.calls==1);
 assert(bleRecoveryRtc.phase==4&&bleRecoveryRtc.device==0&&bleRecoveryRtc.age_ms==1358846);
 assert(bleRecoveryRtc.pending==1&&cielo96::recovery::valid(bleRecoveryRtc));
 // Simulated software boot: hardware RTC survives, RAM supervisor is reset.
 bleRecoveryRtc.pending=0;cielo96::recovery::seal(bleRecoveryRtc);bleSupervisor={};
 bleRecoveryTick();assert(ESP.calls==2);
 bleRecoveryRtc.pending=0;cielo96::recovery::seal(bleRecoveryRtc);bleSupervisor={};
 bleRecoveryTick();bleRecoveryTick();assert(ESP.calls==2&&bleRecoveryExhausted);
 cielo96::sample.heartbeat_age_ms=113;cielo96::sample.phase_age_ms=113;
 bleRecoveryTick();assert(ESP.calls==2&&!bleRecoveryExhausted);
}
'''
with tempfile.TemporaryDirectory() as d:
 source=Path(d)/'test.cpp';source.write_text(fixture+function+main)
 binary=str(Path(d)/'test')
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-Iinclude',str(source),'-o',binary],check=True)
 subprocess.run([binary],check=True)
print('PASS actual bleRecoveryTick: report 41 restarts, OTA/rescue never restarts, budget halts loop')
