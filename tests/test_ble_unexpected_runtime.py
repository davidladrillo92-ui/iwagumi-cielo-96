"""Compile and execute the real unexpected-link-loss handler with simulated hardware."""
from pathlib import Path
import subprocess,tempfile
s=Path('src/main.cpp').read_text()
fn=s[s.index('static void bleUnexpectedDisconnectTick(){'):s.index('static void lightingTask(){')]
fixture=r'''
#include <cstdint>
#include <string>
#include <cassert>
using String=std::string;
uint32_t millis(){return 7000;}
enum class OtaPhase {Idle,Uploading};
struct {OtaPhase phase=OtaPhase::Idle;} otaTxn;
bool bleReady=true,rescueOpen=false,otaBusy=false,pvControlDesired=true,pvOffObligation=false;
bool pvDisconnectInterlock=false,lastPvDesired=true;
bool cieloChRun=true,cieloChWasDriven=true,cieloChReturnPending=false,cieloChAwaitResponse=false;
uint32_t pvOffNextTry=0,pvOffInFlightSeq=3,observedUnexpectedDisconnect[2]{};
uint32_t isolatedChDisconnects=0,pvDisconnectOffSafetyEpisodes=0;
enum class PvSafetyState {OffPending,Active};
PvSafetyState pvSafety=PvSafetyState::Active;
unsigned records=0,returns=0,cancels=0;
void record(const char*,const String&){records++;}
void cieloRequestReturn(const char*){returns++;cieloChReturnPending=true;}
struct {void cancel(){cancels++;}} chReconnect;
namespace cielo {namespace ble {enum class Device {PezVerde,Chihiros};}}
struct {void cancel_pending(cielo::ble::Device){cancels++;}} queue;
namespace cielo96 {
struct Link {uint32_t unexpected_disconnects=0;};
Link links[2]{};
Link ble_link_status(uint8_t d){return links[d];}
}
'''
checks=r'''
int main(){
 bleUnexpectedDisconnectTick();assert(!pvOffObligation&&!returns);
 cielo96::links[1].unexpected_disconnects=1;
 bleUnexpectedDisconnectTick();
 assert(!pvOffObligation&&!pvDisconnectInterlock&&pvOffNextTry==0);
 assert(isolatedChDisconnects==1&&pvDisconnectOffSafetyEpisodes==0);
 assert(!cieloChRun&&returns==1&&cieloChReturnPending&&pvSafety==PvSafetyState::Active);
 auto prev=records;bleUnexpectedDisconnectTick();assert(records==prev&&returns==1);
 otaBusy=true;cielo96::links[0].unexpected_disconnects=1;
 bleUnexpectedDisconnectTick();assert(observedUnexpectedDisconnect[0]==0);
 otaBusy=false;bleUnexpectedDisconnectTick();assert(observedUnexpectedDisconnect[0]==1);
 assert(pvOffObligation&&pvDisconnectInterlock&&pvOffNextTry==7000&&returns==1);
 assert(pvDisconnectOffSafetyEpisodes==1&&isolatedChDisconnects==1);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.cpp';p.write_text(fixture+fn+checks)
 out=str(Path(d)/'test')
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',str(p),'-o',out],check=True)
 subprocess.run([out],check=True)
print('PASS actual unexpected disconnect handler: isolated CH, PV-only OFF interlock, OTA exclusion, dedup')
