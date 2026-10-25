"""Execute the production retry gate, with BLE represented only by its health snapshot."""
from pathlib import Path
import subprocess,tempfile
s=Path('src/main.cpp').read_text();a=s.index('static void chReconnectTick(){');function=s[a:s.index('static void bleRecoveryTick(){',a)]
fixture=r'''
#include "cielo_autonomy_policy.h"
#include <cassert>
#include <string>
unsigned clockNow=40000;unsigned millis(){return clockNow;}
unsigned records=0;void record(const char*,const char*){records++;}
cielo96::autonomy::Retry chReconnect;
bool cieloAlwaysOn=true,bleReady=true,rescueOpen=false,bleRecoveryBootPvSafe=false;
bool currentBootMarkedHealthy=true,otaBusy=false,customResumePending=false;
bool cieloChReturnPending=false,cieloChAwaitResponse=false,chSelectiveHold=false,healthy=true,clockOk=true,labArmed=false;
bool timeOK(){return clockOk;}bool otaBleHealthy(){return healthy;}bool labTxEnabled(){return labArmed;}
struct {bool scanPending=false,probePending=false;} lab;
struct {bool valid=true;} profile;
namespace cielo96 {bool profileValid(decltype(profile)&p){return p.valid;}}
enum class OtaPhase {Idle,Preparing,Ready,Uploading};struct {OtaPhase phase=OtaPhase::Idle;} otaTxn;
namespace cielo {namespace ble {enum class Device {Chihiros};}}
struct {bool active=false,pendingCh=false;bool busy(){return active;}bool pending(cielo::ble::Device){return pendingCh;}} queue;
struct {bool works=true;unsigned writes=0;unsigned putBool(const char*,bool){writes++;return works?1:0;}} prefs;
bool cieloChRun=false,lastChDesired=true;unsigned cieloChConsecutiveFailures=2,lastScheduleAt=0;
'''
main=r'''
int main(){
 chReconnect.arm(0);assert(chReconnect.pending);
 // Never take ownership during OTA, manual transmission, recovery or a pending return.
 for(auto p:{OtaPhase::Preparing,OtaPhase::Ready,OtaPhase::Uploading}){
  otaTxn.phase=p;chReconnectTick();assert(records==0&&prefs.writes==0);
 }
 otaTxn.phase=OtaPhase::Idle;
 for(bool*guard:{&rescueOpen,&bleRecoveryBootPvSafe,&otaBusy,&lab.scanPending,&lab.probePending,
  &customResumePending,&cieloChReturnPending,&cieloChAwaitResponse,&chSelectiveHold,&queue.active,&queue.pendingCh,&labArmed}){
  *guard=true;chReconnectTick();assert(records==0);*guard=false;
 }
 for(bool*guard:{&bleReady,&currentBootMarkedHealthy,&healthy,&clockOk,&profile.valid}){
  *guard=false;chReconnectTick();assert(records==0);*guard=true;
 }
 prefs.works=false;chReconnectTick();assert(records==0&&chReconnect.pending&&!cieloChRun);
 prefs.works=true;clockNow+=30000;chReconnectTick();assert(records==1&&cieloChRun&&!lastChDesired);
 assert(!chReconnect.pending&&chReconnect.attempts==1&&cieloChConsecutiveFailures==0);
 chReconnectTick();assert(records==1); // no repeat every loop
 chReconnect.arm(clockNow);cieloAlwaysOn=false;chReconnectTick();assert(!chReconnect.pending);
 clockNow+=600000;chReconnectTick();assert(records==1); // explicit pause remains respected
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.cpp';p.write_text(fixture+function+main);out=str(Path(d)/'test')
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-Iinclude',str(p),'-o',out],check=True)
 subprocess.run([out],check=True)
print('PASS actual Chihiros recovery: independent retry, all inhibition gates, NVS failure, deliberate pause')
