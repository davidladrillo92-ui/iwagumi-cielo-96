"""Exercise the real CIELO persistent-link release policy without ESP32 hardware."""
from pathlib import Path
import subprocess,tempfile
s=Path('src/cielo_ble_worker.cpp').read_text()
h=Path('include/cielo_ble_worker.h').read_text()
start=s.index('void releaseIdleLinks(){')
release=s[start:s.index('uint16_t chMessageId=',start)]
start=s.index('void ble_keep_links(bool pv,bool ch){')
keep=s[start:s.index('bool ble_dispatch(',start)]
assert 'unexpected_disconnects' in h
assert 'if((!keepLinks[device].load() || !success)' in s
assert 'releaseTarget(1,true); // Nunca quedar bloqueando My Chihiros.' in s
source=r'''
#include <atomic>
#include <cassert>
#include <cstdint>
struct Client {bool active=false;bool isConnected(){return active;}};
struct Target {Client* client=nullptr;};
Client clients[2]{};Target targets[2]{{&clients[0]},{&clients[1]}};
std::atomic<bool> keepLinks[2]{};
unsigned releases[2]{};
void releaseTarget(uint8_t device,bool user){
 assert(!user);releases[device]++;clients[device].active=false;
}
'''+release+keep+r'''
int main(){
 clients[0].active=true;clients[1].active=true;
 ble_keep_links(true,true);releaseIdleLinks();
 assert(!releases[0]&&!releases[1]); // both links remain alive
 ble_keep_links(true,false);releaseIdleLinks();
 assert(!releases[0]&&releases[1]==1); // CH given to My Chihiros
 clients[1].active=true;ble_keep_links(false,false);releaseIdleLinks();
 assert(releases[0]==1&&releases[1]==2); // OTA or explicit relinquish
 releaseIdleLinks();assert(releases[0]==1&&releases[1]==2); // no redundant release
 clients[0].active=true;ble_keep_links(true,false);releaseIdleLinks();
 assert(releases[0]==1); // held again
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.cpp';p.write_text(source);out=str(Path(d)/'test')
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',str(p),'-o',out],check=True)
 subprocess.run([out],check=True)
print('PASS persistent BLE: reuse, selective release, OTA, no duplicate disconnect')
