"""Execute the actual fresh-target dispatch block with a real host scheduler."""
from pathlib import Path
import subprocess,tempfile
s=Path('src/main.cpp').read_text();a=s.index('  if(autoApply315&&isolated315[d].waiting&&!(d==0&&pvOffObligation)){');b=s.index('  if(chSelectiveHold && cmd.device',a);block=s[a:b]
fixture=r'''
#include <cassert>
#include "cielo_recovery315_dev2.h"
#include "cielo_ble_queue.h"
using namespace cielo::ble;
cielo96::dev2::Recovery isolated315[2];
Scheduler queue;
bool autoApply315=true,pvOffObligation=false,otaBusy=false,trusted=true,lastChDesired=true,lastPvDesired=true;
bool clockEvidenceTrusted(){return trusted;}
struct Profile{bool valid=true;}profile;
namespace cielo96{bool profileValid(Profile& p){return p.valid;}}
enum class OtaPhase{Idle,Preparing};struct {OtaPhase phase=OtaPhase::Idle;}otaTxn;
struct CH{uint8_t r,g,b;};struct PV{uint8_t r,g,b,brightness;};
CH currentCh{0,0,0},prevCh{};PV currentPv{0,0,0,0},prevPv{};
CH desiredCh(){return currentCh;}PV desiredPv(){return currentPv;}
uint32_t lastPvDesiredAt=0,lastChDesiredAt=0;
void refresh(Command& cmd,uint8_t d,uint32_t now){
'''
checks=r'''
}
int main(){
 isolated315[1].loss(1000);assert(queue.submit(Device::Chihiros,Action::Apply,40,30,20,0,6000));
 Command cmd{};assert(queue.start_next(6000,cmd));refresh(cmd,1,6000);assert(cmd.r==0&&cmd.g==0&&cmd.b==0&&lastChDesiredAt==6000&&queue.busy());
 assert(queue.complete(cmd.sequence,true,6001));
 isolated315[0].loss(1000);currentPv={11,22,33,15};assert(queue.submit(Device::PezVerde,Action::Apply,5,5,5,1,6000));
 assert(queue.start_next(6001,cmd));refresh(cmd,0,6001);assert(cmd.r==11&&cmd.g==22&&cmd.b==33&&cmd.brightness==15&&lastPvDesiredAt==6001);assert(queue.complete(cmd.sequence,true,6002));
 trusted=false;assert(queue.submit(Device::Chihiros,Action::Apply,50,50,50,0,7000));assert(queue.start_next(7000,cmd));refresh(cmd,1,7000);assert(!queue.busy()&&!queue.pending(Device::Chihiros)&&!lastChDesired);
 // OFF safety bypasses the normal trusted-time/recovery gate.
 pvOffObligation=true;assert(queue.submit(Device::PezVerde,Action::Apply,0,0,0,0,7000));assert(queue.start_next(7000,cmd));refresh(cmd,0,7000);assert(queue.busy()&&cmd.brightness==0);assert(queue.complete(cmd.sequence,true,7001));
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'fresh.cpp';p.write_text(fixture+block+checks);exe=str(Path(d)/'test')
 subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror','-Iinclude',str(p),'-o',exe],check=True)
 subprocess.run([exe],check=True)
print('PASS actual dispatch block: latest CH/PV target, invalid clock cancellation, urgent OFF bypass')
