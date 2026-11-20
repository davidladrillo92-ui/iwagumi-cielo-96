"""Exercise actual new checkpoint and lamp-health functions, without ESP or flash."""
from pathlib import Path
import tempfile, subprocess
s=Path('src/main.cpp').read_text()
lamp=s[s.index('static bool lampControlCritical(uint8_t device){'):s.index('// A start request verifies BOTH')]
checkpoint=s[s.index('static void profileCheckpointTick(){'):s.index('static void physicalButtonTask(){')]
fixture=r'''
#include "cielo_recovery313.h"
#include <cassert>
#include <cstring>
uint32_t now=600000,secs=600;uint32_t millis(){return now;}uint32_t uptimeSeconds(){return secs;}
bool currentBootMarkedHealthy=true,otaCandidateBoot=false,otaBusy=false,rescueOpen=false;
enum class OtaPhase {Idle,Preparing};struct {OtaPhase phase=OtaPhase::Idle;}otaTxn;
struct {bool txArmed=false;}lab;
bool cieloAlwaysOn=true,pvControlDesired=true,cieloChRun=true,cieloPvRun=true,pvOffObligation=false,pvDisconnectInterlock=false;
uint32_t pvOffFailures=0,pvOffLastGatt=0;
bool fresh[2]={true,true},offWait=false;
bool bleCommunicationRecent(uint8_t d){return fresh[d];}bool pvWaitingOffCheck(){return offWait;}
struct Image {uint32_t generation=22;}profile;
namespace cielo96 {struct Link {bool connected=true;};Link link;Link ble_link_status(uint8_t){return link;}
namespace curves {bool curveValid(const Image&){return true;}}}
struct {bool writable(){return true;}}curveFs;
const char*loadedCurveBackend="LITTLEFS_AB";
uint32_t knownProfileGeneration=0,profileCheckpointLastTry=0;
bool profileCheckpointAttempted=false,profileCheckpointWriteFailed=false,profileFallbackSuspected=false,curveSaveInProgress=false;
bool low=true;bool nvsPreserveCurveHeadroom(){return low;}
struct {uint32_t val=0,attempts=0;bool fail=false;
 size_t putUInt(const char*,uint32_t n){++attempts;if(fail)return 0;val=n;return 4;}
 uint32_t getUInt(const char*,uint32_t){return val;}}prefs;
'''
checks=r'''
int main(){
 assert(!lampControlCritical(0)&&!lampControlCritical(1));
 fresh[0]=false;pvOffObligation=true;pvOffLastGatt=now-300000;
 assert(!lampControlCritical(0)); // deadline plus short allowance
 now+=15001;assert(lampControlCritical(0));
 pvOffLastGatt=now-1000;pvDisconnectInterlock=true;assert(lampControlCritical(0));
 cieloChRun=false;assert(lampControlCritical(1));cieloAlwaysOn=false;assert(!lampControlCritical(1));
 cieloAlwaysOn=true;secs=599;assert(!lampControlCritical(1));secs=600;
 otaBusy=true;assert(!lampControlCritical(1));otaBusy=false;
 profileCheckpointTick();assert(profileCheckpointWriteFailed&&prefs.attempts==0);
 low=false;now+=299999;profileCheckpointTick();assert(prefs.attempts==0);
 now++;profileCheckpointTick();assert(knownProfileGeneration==22&&!profileCheckpointWriteFailed&&prefs.attempts==1);
 now+=300000;profileCheckpointTick();assert(prefs.attempts==1);
 profile.generation=9;profileFallbackSuspected=true;profileCheckpointTick();assert(prefs.attempts==1&&profileFallbackSuspected);
 profile.generation=23;otaCandidateBoot=true;profileCheckpointTick();assert(prefs.attempts==1);otaCandidateBoot=false;
 prefs.fail=true;profileCheckpointTick();assert(knownProfileGeneration==22&&profileCheckpointWriteFailed&&prefs.attempts==2);
 prefs.fail=false;now+=300000;profileCheckpointTick();assert(knownProfileGeneration==23&&prefs.attempts==3&&!profileCheckpointWriteFailed);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'runtime.cpp';p.write_text(fixture+lamp+checkpoint+checks);out=str(Path(d)/'runtime')
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-Iinclude',str(p),'-o',out],check=True)
 subprocess.run([out],check=True)
print('PASS 3.13 actual lamp health and generation checkpoint: planned OFF, paused, grace, OTA, low NVS, old profile, no-op and retry')
