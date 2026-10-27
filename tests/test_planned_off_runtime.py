"""Run actual report89 resting-state gate, including worker/link/safety conditions."""
from pathlib import Path
import subprocess,tempfile
src=Path('src/main.cpp').read_text()
fn=src[src.index('static bool pvWaitingOffCheck(){'):src.index('static String cieloModeJson(){')]
code=r'''
#include "cielo_ble_presence.h"
#include <cassert>
#include <initializer_list>
uint32_t now=127000;uint32_t millis(){return now;}
bool cieloPvRun=true,pvControlDesired=true,pvOffObligation=true,pvDisconnectInterlock=false;
bool cieloWatchdogTripped=false,bleRecoveryBootPvSafe=false,bleReady=true;
uint32_t pvOffLastGatt=1000,cieloLastSuccessfulPvAt=1000,cieloPvConsecutiveFailures=0,pvOffFailures=0;
enum class PvSafetyState{OffGattOk,OffPending};PvSafetyState pvSafety=PvSafetyState::OffGattOk;
namespace cielo96{
struct {bool connected=true;}link;
struct {uint32_t heartbeat_age_ms=0;}worker;
auto ble_link_status(int)->decltype(link){return link;}
auto ble_worker_snapshot()->decltype(worker){return worker;}
}
'''+fn+r'''
int main(){
 assert(pvWaitingOffCheck());assert(!cielo96::presence::recent(true,1000,now,0));
 for(auto p:{&cieloPvRun,&pvControlDesired,&pvOffObligation,&bleReady,&cielo96::link.connected}){*p=false;assert(!pvWaitingOffCheck());*p=true;}
 for(auto p:{&pvDisconnectInterlock,&cieloWatchdogTripped,&bleRecoveryBootPvSafe}){*p=true;assert(!pvWaitingOffCheck());*p=false;}
 pvSafety=PvSafetyState::OffPending;assert(!pvWaitingOffCheck());pvSafety=PvSafetyState::OffGattOk;
 pvOffFailures=1;assert(!pvWaitingOffCheck());pvOffFailures=0;
 cieloPvConsecutiveFailures=1;assert(!pvWaitingOffCheck());cieloPvConsecutiveFailures=0;
 cielo96::worker.heartbeat_age_ms=15000;assert(!pvWaitingOffCheck());cielo96::worker.heartbeat_age_ms=0;
 now=301000;assert(!pvWaitingOffCheck());now=300999;assert(pvWaitingOffCheck());
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.cpp';p.write_text(code);exe=str(Path(d)/'test')
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-Iinclude',str(p),'-o',exe],check=True)
 subprocess.run([exe],check=True)
print('PASS report89 planned-OFF gate: no false live status,300s boundary, link loss, safety interlocks, radio/worker failures')
