"""Run the real idle release policy: no needless disconnect between curve updates."""
from pathlib import Path
import re,subprocess,tempfile
s=Path('src/cielo_ble_worker.cpp').read_text()
constant=re.search(r'static constexpr uint32_t BLE_IDLE_RELEASE_MS=\d+UL;',s).group()
a=s.index('void releaseIdleLinks(){');f=s[a:s.index('uint16_t chMessageId=',a)]
source=r'''
#include <stdint.h>
#include <atomic>
std::atomic<bool> keepLink[2]{};
#include <cassert>
uint32_t clockNow=0;uint32_t millis(){return clockNow;}
uint32_t linkTouched[2]{};unsigned releases[2]{};
void releaseTarget(uint8_t device,bool user){assert(!user);releases[device]++;linkTouched[device]=0;}
'''+constant+f+r'''
int main(){
 linkTouched[0]=1;linkTouched[1]=100;
 for(auto now:{5001U,30001U,60001U,89999U}){clockNow=now;releaseIdleLinks();assert(!releases[0]&&!releases[1]);}
 // A normal curve update extends only its own idle deadline.
 linkTouched[0]=60001;clockNow=90100;releaseIdleLinks();assert(!releases[0]&&releases[1]==1);
 clockNow=150001;releaseIdleLinks();assert(releases[0]==1&&releases[1]==1);
 releaseIdleLinks();assert(releases[0]==1&&releases[1]==1);
 keepLink[0]=true;linkTouched[0]=1;clockNow=900001;releaseIdleLinks();assert(releases[0]==1);keepLink[0]=false;
 linkTouched[0]=0xfffffff0UL;clockNow=89984;releaseIdleLinks();assert(releases[0]==2);
}
'''
source=source.replace('#include <cassert>','#include <cassert>\n#include <initializer_list>')
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.cpp';p.write_text(source);out=str(Path(d)/'test')
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',str(p),'-o',out],check=True)
 subprocess.run([out],check=True)
assert 'releaseTarget(1,true); // Nunca quedar bloqueando My Chihiros.' in s
print('PASS actual idle-release policy: reuse across curve ticks, independent deadlines, rollover, immediate explicit return')
