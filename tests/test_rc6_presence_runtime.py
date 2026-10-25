"""Execute worker reconciliation and callbacks with an unplugged fake client."""
from pathlib import Path
import subprocess,tempfile
s=Path('src/cielo_ble_worker.cpp').read_text()
reconcile=s[s.index('void reconcileLinks(){'):s.index('void releaseIdleLinks(){')]
callbacks=s[s.index('struct LinkCallbacks:'):s.index('LinkCallbacks linkCallbacks[2]')]
prefix=r'''
#include <atomic>
#include <cstdint>
#include <cassert>
uint32_t millis(){return 12345;}
struct BLEClient {bool live=false;bool isConnected(){return live;}bool pending=false;bool cieloConnectPending(){return pending;}};
struct BLEClientCallbacks {
 virtual void onConnect(BLEClient*){};virtual void onDisconnect(BLEClient*){};
};
struct {bool connected=false,idleReleased=false;std::atomic<bool>expectedRelease{false};uint32_t lastDisconnect=0,pendingSince=0;} shared[2];
std::atomic<int> connectState[2]{};int lastStackError[2]{};
struct CallbackCounter {std::atomic<uint32_t> connected{0},disconnected{0},lastConnect{0},lastDisconnect{0};};
CallbackCounter callbackCounters[2]{};
struct Target {BLEClient*client=nullptr;void*writer=nullptr;bool manualEntered=false,pvOn=false;} targets[2];
'''
main=r'''
int main(){
 BLEClient pv,ch;ch.live=true;int writer=0;
 targets[0]={&pv,&writer,true,true};targets[1]={&ch,&writer,true,true};
 shared[0].connected=shared[1].connected=true;
 reconcileLinks();
 assert(!shared[0].connected&&shared[0].lastDisconnect==12345);
 assert(!targets[0].writer&&!targets[0].manualEntered&&!targets[0].pvOn);
 assert(shared[1].connected&&targets[1].writer&&targets[1].pvOn);
 LinkCallbacks cb(1);cb.onDisconnect(&ch);
 assert(!shared[1].connected);
 assert(shared[1].lastDisconnect==12345);
 shared[1].expectedRelease=true;shared[1].idleReleased=true;cb.onDisconnect(&ch);assert(shared[1].idleReleased);
 cb.onConnect(&ch);assert(shared[1].connected&&!shared[1].idleReleased);
 assert(callbackCounters[1].connected==1&&callbackCounters[1].disconnected==2);
 assert(callbackCounters[1].lastConnect==12345&&callbackCounters[1].lastDisconnect==12345);
 pv.pending=true;reconcileLinks();assert(shared[0].pendingSince==12345);pv.pending=false;reconcileLinks();assert(!shared[0].pendingSince);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.cpp';p.write_text(prefix+callbacks+reconcile+main);out=str(Path(d)/'test')
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',str(p),'-o',out],check=True)
 subprocess.run([out],check=True)
main=Path('src/main.cpp').read_text()
assert 'presence::refreshDue(now,lastPvDesiredAt)' in main
assert 'presence::refreshDue(now,lastChDesiredAt)' in main
assert 'bleCommunicationRecent(0)' in main and 'bleCommunicationRecent(1)' in main
assert s.index('reconcileLinks();') < s.index('xQueueReceive(requests,&c,')
assert 'cieloConnectPending()' in s
assert 'releaseTarget(device,false)' in s
print('PASS unplugged worker reconciliation, disconnect callbacks and 60 s refresh, idle release and pending SDK request wiring')
