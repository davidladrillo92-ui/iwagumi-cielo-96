"""Execute actual firmware work-light handlers with simulated clock/radio/server."""
from pathlib import Path
import subprocess,tempfile,json
src=Path('src/main.cpp').read_text()
wrappers=src[src.index('static cielo96::worklight::Levels programLevels()'):src.index('static void loadProfile(){')]
handlers=src[src.index('static const char* workLightBlockReason'):src.index('static void statusJson(){')]
fixture=r'''
#include "cielo_worklight.h"
#include "cielo_curve_policy.h"
#include "cielo_ble_queue.h"
#include "cielo_night_safety.h"
#include <cassert>
#include <string>
#include <iostream>
#include <type_traits>
struct String {
 std::string v;String()=default;String(const char*s):v(s){}String(const std::string&s):v(s){}
 template<class T,typename std::enable_if<std::is_integral<T>::value,int>::type=0>
 String(T n):v(std::to_string(n)){}
 String& operator+=(const String& b){v+=b.v;return *this;}
 friend String operator+(String a,const String& b){return a+=b;}
 bool operator!=(const char*s)const{return v!=s;}
};
String jsonQuote(const String&s){return String("\"")+s+String("\"");}
uint32_t now=1000000;uint32_t millis(){return now;}
enum class OtaPhase{Idle,Ready};struct {OtaPhase phase=OtaPhase::Idle;}otaTxn;
bool rescueOpen=false,otaBusy=false,currentBootMarkedHealthy=true,bleReady=true,clockOk=true,coex=true;
bool cieloAlwaysOn=true,cieloPvRun=true,cieloChRun=true,pvControlDesired=true;
bool cieloWatchdogTripped=false,bleRecoveryBootPvSafe=false,pvDisconnectInterlock=false,chSelectiveHold=false;
bool customResumePending=false,cieloChReturnPending=false,cieloChAwaitResponse=false;
struct {bool txArmed=false,scanPending=false,probePending=false;}lab;
unsigned contrastStage=0;bool cadenceTrial15=false,pezManual=false,chiManual=false,pvOffObligation=true;
enum class PvSafetyState{OffPending,OffGattOk};PvSafetyState pvSafety=PvSafetyState::OffGattOk;
cielo96::ProfileImage profile;
namespace cielo96 {struct Worker{uint32_t heartbeat_age_ms=0;};Worker worker;Worker ble_worker_snapshot(){return worker;}}
bool timeOK(){return clockOk;}bool wifiCoexistReady(){return coex;}
bool recent[2]={true,true};bool bleCommunicationRecent(int i){return recent[i];}
cielo96::worklight::Session workLight;uint32_t workLightTicks=0;String workLightEndReason="Sin sesion previa";
cielo::ble::Scheduler queue;
bool lastPvDesired=true,lastChDesired=true;uint32_t lastPvDesiredAt=now,lastChDesiredAt=now,lastScheduleAt=0;
cielo::PvValue prevPv{0,0,0,0};cielo::ChiValue prevCh{0,0,0};
cielo96::worklight::Levels base{{0,0,0,0},{0,0,0}};
cielo::PvValue programPv(){return clockOk?base.pv:cielo::PvValue{0,0,0,0};}
cielo::ChiValue programCh(){return clockOk?base.ch:cielo::ChiValue{0,0,0};}
struct {String confirm="INICIAR_MANTENIMIENTO";String arg(const char*){return confirm;}}srv;
unsigned events=0;int http=0;String body;
void record(const char*,const String&){events++;}
void jsonSend(int code,const String& s){http=code;body=s;}
void fail(int code,const char*s){http=code;body=s;}
'''
checks=r'''
int main(){
 profile.pvCount=profile.chCount=2;profile.pv[1].minute=profile.ch[1].minute=1440;cielo96::profileSeal(profile);
 assert(!workLightBlockReason(true));auto saved=profile;
 workLightStart();assert(http==200&&workLight.active());
 assert(!lastPvDesired&&!lastChDesired);auto begun=workLight.elapsed(now);
 now+=60000;workLightStart();assert(http==200&&workLight.elapsed(now)==begun+60000);
 now+=540000;workLightTick();assert(workLight.phase()==cielo96::worklight::Phase::Holding);
 assert(desiredPv().brightness==15&&desiredCh().r==50);
 assert(!cielo96::safety::offRequired(true,true,true,desiredPv().brightness));
 workLightStop();assert(workLight.phase()==cielo96::worklight::Phase::Returning);
 now+=30000;workLightStop();assert(workLight.remaining(now)==90000);
 now+=90000;workLightTick();assert(!workLight.active());
 assert(desiredCh().r==0&&cielo96::safety::offRequired(true,true,true,desiredPv().brightness));
 assert(memcmp(&saved,&profile,sizeof(profile))==0);
 // Every conflict below is checked by the actual production gate.
 bool* flags[]={&rescueOpen,&otaBusy,&cieloWatchdogTripped,&bleRecoveryBootPvSafe,&pvDisconnectInterlock,
 &chSelectiveHold,&customResumePending,&cieloChReturnPending,&cieloChAwaitResponse,&lab.txArmed,
 &lab.scanPending,&lab.probePending,&cadenceTrial15,&pezManual,&chiManual};
 for(auto f:flags){*f=true;assert(workLightBlockReason(true));workLightStart();assert(http==423&&!workLight.active());*f=false;}
 for(auto f:{&currentBootMarkedHealthy,&bleReady,&clockOk,&coex,&cieloAlwaysOn,&cieloPvRun,&cieloChRun,&pvControlDesired}){
  *f=false;assert(workLightBlockReason(true));*f=true;
 }
 for(int i=0;i<2;i++){recent[i]=false;assert(workLightBlockReason(true));recent[i]=true;}
 cielo96::worker.heartbeat_age_ms=15000;assert(workLightBlockReason(true));cielo96::worker.heartbeat_age_ms=0;
 pvSafety=PvSafetyState::OffPending;assert(workLightBlockReason(true));pvSafety=PvSafetyState::OffGattOk;
 for(unsigned stage:{1u,2u,4u}){contrastStage=stage;assert(workLightBlockReason(true));}contrastStage=0;
 otaTxn.phase=OtaPhase::Ready;assert(workLightBlockReason(true));otaTxn.phase=OtaPhase::Idle;
 profile.checksum^=1;assert(workLightBlockReason(true));profile=saved;
 // Cancel on unsafe runtime conditions; old queued maintenance values removed.
 workLightStart();assert(workLight.active());
 queue.submit(cielo::ble::Device::PezVerde,cielo::ble::Action::Apply,96,150,210,15,now);
 clockOk=false;workLightTick();assert(!workLight.active()&&!queue.pending(cielo::ble::Device::PezVerde));clockOk=true;
 // Existing unconfirmed OFF can finish during the final dark ramp without aborting it.
 workLightStart();now+=600000;workLightTick();workLightStop();now+=119000;
 pvSafety=PvSafetyState::OffPending;assert(desiredPv().brightness==0);workLightTick();assert(workLight.active());
 now+=1000;workLightTick();assert(!workLight.active());pvSafety=PvSafetyState::OffGattOk;
 base={{240,238,230,25},{70,65,60}};pvOffObligation=false;
 workLightStart();now+=600000;workLightTick();workLightStop();now+=120000;workLightTick();
 assert(!workLight.active()&&desiredCh().r==70&&desiredPv().brightness==25);
 std::cout<<workLightJson().v<<"\n";
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.cpp';p.write_text(fixture+wrappers+handlers+checks);exe=str(Path(d)/'test')
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-Iinclude',str(p),'-o',exe],check=True)
 data=json.loads(subprocess.check_output([exe],text=True))
 assert data['active'] is False and data['requested_ch']==[70,65,60]
 assert data['saved_profile_unchanged'] and not data['resume_after_reboot']
print('PASS actual worklight handlers: morning/daytime, idempotence, all admission guards, unsafe cancellation, queued values cleared, OFF priority and profile bytes unchanged')
