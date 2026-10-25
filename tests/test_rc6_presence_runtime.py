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
struct NimBLEClient {bool live=false;bool isConnected(){return live;}};
struct NimBLEClientCallbacks {
 virtual void onConnect(NimBLEClient*){};virtual void onConnectFail(NimBLEClient*,int){};virtual void onDisconnect(NimBLEClient*,int){};
};
struct {bool connected=false;uint32_t lastDisconnect=0;} shared[2];
std::atomic<int> connectState[2]{};int lastStackError[2]{};
struct Target {NimBLEClient*client=nullptr;void*writer=nullptr;bool manualEntered=false,pvOn=false;} targets[2];
'''
main=r'''
int main(){
 NimBLEClient pv,ch;ch.live=true;int writer=0;
 targets[0]={&pv,&writer,true,true};targets[1]={&ch,&writer,true,true};
 shared[0].connected=shared[1].connected=true;
 reconcileLinks();
 assert(!shared[0].connected&&shared[0].lastDisconnect==12345);
 assert(!targets[0].writer&&!targets[0].manualEntered&&!targets[0].pvOn);
 assert(shared[1].connected&&targets[1].writer&&targets[1].pvOn);
 LinkCallbacks cb(1);cb.onDisconnect(&ch,8);
 assert(!shared[1].connected&&lastStackError[1]==8&&connectState[1]==-1);
 assert(shared[1].lastDisconnect==12345);
 cb.onConnectFail(&ch,13);assert(!shared[1].connected&&lastStackError[1]==13);
 cb.onConnect(&ch);assert(shared[1].connected&&connectState[1]==1);
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
assert 'setConnectionParams(24,40,0,400)' in s
print('PASS unplugged worker reconciliation, disconnect callbacks and 30 s refresh wiring')
