"""Exercise actual sendPez when midnight arrives during connection or a write."""
from pathlib import Path
import subprocess
import tempfile
s = Path('src/cielo_ble_worker.cpp').read_text()
function = s[s.index('bool sendPez('):s.index('bool sendCh(')]
fixture = r'''
#include "cielo_codec.h"
#include "cielo_ble_queue.h"
#include <atomic>
#include <cassert>
#include <cstring>
#include <vector>
using namespace cielo96;
struct Target {bool pvOn=false;} targets[2];
std::atomic<bool> pvOffRequired{false};bool offWrittenInCurrentOperation=false;
bool offDuringConnect=false;unsigned offAfterFrame=0;
std::vector<std::vector<uint8_t>> sent;
bool connect(Target&,const char*&){if(offDuringConnect)pvOffRequired=true;return true;}
bool put(Target&,const uint8_t*p,size_t n,const char*&){sent.emplace_back(p,p+n);if(sent.size()==offAfterFrame)pvOffRequired=true;return true;}
'''
main = r'''
int main(){
 cielo::ble::Command c{};c.r=240;c.g=238;c.b=230;c.brightness=25;const char*err="OK";
 uint8_t off[9];codec::pv_power(false,off);
 for(unsigned phase=0;phase<3;phase++){
  targets[0].pvOn=false;pvOffRequired=false;offWrittenInCurrentOperation=false;sent.clear();
  offDuringConnect=phase==0;offAfterFrame=phase;
  assert(sendPez(c,err));assert(offWrittenInCurrentOperation);
  assert(sent.back().size()==9&&memcmp(sent.back().data(),off,9)==0);
  assert(sent.size()==phase+1); // no stale brightness after the OFF interlock
 }
 offDuringConnect=false;offAfterFrame=0;sent.clear();pvOffRequired=false;targets[0].pvOn=false;
 assert(sendPez(c,err)&&sent.size()==3);
}
'''
with tempfile.TemporaryDirectory() as d:
    p=Path(d)/'test.cpp';p.write_text(fixture+function+main);binary=str(Path(d)/'test')
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-Iinclude',str(p),'-o',binary],check=True)
    subprocess.run([binary],check=True)
print('PASS actual sendPez: OFF interlock during connect, power-on or RGB; normal protocol unchanged')
