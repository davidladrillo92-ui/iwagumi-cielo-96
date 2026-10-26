"""Execute actual firmware loadProfile; migration must preserve NVS and bytes."""
from pathlib import Path
import subprocess
import tempfile

source = Path('src/main.cpp').read_text()
load = source[source.index('static void loadProfile(){'):source.index('struct CurveNvsStore {')]
fixture = r'''
#include "cielo_curve_policy.h"
#include "profiles_generated.h"
#include <cassert>
#include <cstring>
#include <string>
#include <map>
#include <vector>
#include <iostream>
using namespace cielo96;
struct String {
 std::string v;String(const char*p):v(p){}String(uint32_t n):v(std::to_string(n)){}
 friend String operator+(const String&a,const String&b){return String((a.v+b.v).c_str());}
};
void record(const char*,const String&){}
struct Prefs {
 std::map<std::string,std::vector<uint8_t>> data;
 size_t getBytesLength(const char*k){auto it=data.find(k);return it==data.end()?0:it->second.size();}
 size_t getBytes(const char*k,void*out,size_t n){assert(data[k].size()==n);memcpy(out,data[k].data(),n);return n;}
 void fixture(const char*k,const ProfileImage&p){data[k]=std::vector<uint8_t>((const uint8_t*)&p,(const uint8_t*)&p+sizeof(p));}
}prefs;
struct Fs {
 ProfileImage file{},copied{};bool exists=false,little=true,ready_=true,blocked=false,accept=true;
 unsigned migrations=0;
 bool newest(ProfileImage&out){if(exists)out=file;return exists;}
 bool isLittlefs(){return little;}bool ready(){return ready_;}
 const char*backend(){return "LITTLEFS_AB";}const char*error(){return "TEST_IO_ERROR";}
 void block(const char*){blocked=true;}
 bool migrate(const ProfileImage&p){assert(!blocked);migrations++;copied=p;if(accept){file=p;exists=true;}return accept;}
}curveFs;
namespace cielo96 {namespace profilefs {inline bool newerGeneration(uint32_t a,uint32_t b){return int32_t(a-b)>0;}}}
ProfileImage profile;bool profileRecovery=false;const char*loadedCurveBackend="DEFAULT_RAM";
ProfileImage image(uint32_t gen){ProfileImage p{};p.generation=gen;p.pvCount=38;p.chCount=31;
 for(unsigned i=0;i<38;i++)p.pv[i]={uint16_t(i==37?1440:i*38),12,34,56,uint8_t(i==0||i==37?0:10)};
 for(unsigned i=0;i<31;i++)p.ch[i]={uint16_t(i==30?1440:i*48),uint8_t(i==30?0:20),0,0};
 profileSeal(p);assert(curves::curveValid(p));return p;}
void reset(){prefs={};curveFs={};profileRecovery=false;loadedCurveBackend="DEFAULT_RAM";}
'''
checks = r'''
int main(){
 auto old=image(8),active=image(9),newer=image(10);
 reset();prefs.fixture("curvesA",active);prefs.fixture("curvesB",old);auto nvs=prefs.data;
 loadProfile();assert(curveFs.migrations==1);
 assert(memcmp(&curveFs.copied,&active,sizeof(active))==0&&profile.generation==9);
 assert(!strcmp(loadedCurveBackend,"LITTLEFS_AB")&&!profileRecovery&&prefs.data==nvs);
 loadProfile();assert(curveFs.migrations==1&&prefs.data==nvs); // no boot rewrite
 reset();prefs.fixture("curvesA",active);curveFs.exists=true;curveFs.file=newer;nvs=prefs.data;
 loadProfile();assert(profile.generation==10&&curveFs.migrations==0&&prefs.data==nvs);
 reset();prefs.fixture("curvesA",active);curveFs.exists=true;curveFs.file=old;
 loadProfile();assert(profile.generation==9&&curveFs.migrations==1);
 reset();prefs.fixture("curvesA",active);curveFs.exists=true;curveFs.file=active;
 loadProfile();assert(profile.generation==9&&curveFs.migrations==0&&!strcmp(loadedCurveBackend,"LITTLEFS_AB"));
 reset();prefs.fixture("curvesA",active);curveFs.exists=true;curveFs.file=active;
 curveFs.file.pv[1].r++;profileSeal(curveFs.file);nvs=prefs.data;
 loadProfile();assert(curveFs.blocked&&curveFs.migrations==0&&prefs.data==nvs);
 assert(!strcmp(loadedCurveBackend,"NVS_LEGACY")&&memcmp(&profile,&active,sizeof(active))==0);
 reset();prefs.fixture("curvesA",active);auto conflicting=active;conflicting.pv[1].r++;profileSeal(conflicting);
 prefs.fixture("curvesB",conflicting);loadProfile();assert(curveFs.blocked&&curveFs.migrations==0);
 reset();prefs.fixture("curvesA",active);curveFs.accept=false;nvs=prefs.data;
 loadProfile();assert(profile.generation==9&&curveFs.migrations==1&&!strcmp(loadedCurveBackend,"NVS_LEGACY")&&prefs.data==nvs);
 reset();loadProfile();assert(curveFs.migrations==0&&profile.generation==0&&!profileRecovery);
 reset();prefs.fixture("curvesA",active);prefs.data["curvesA"][0]^=1;
 curveFs.exists=true;curveFs.file=newer;loadProfile();assert(profile.generation==10&&!profileRecovery);
 reset();prefs.fixture("curvesA",active);curveFs.ready_=false;
 loadProfile();assert(curveFs.migrations==0&&profile.generation==9&&!strcmp(loadedCurveBackend,"NVS_LEGACY"));
 std::cout<<"PASS actual loadProfile: exact gen9 migration, reboot no-op, newest selection, conflicts, read failure fallback and NVS unchanged\n";
}
'''
with tempfile.TemporaryDirectory() as d:
    cpp = Path(d) / 'boot.cpp'
    cpp.write_text(fixture + load + checks)
    out = str(Path(d) / 'boot')
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-Iinclude', str(cpp), '-o', out], check=True)
    subprocess.run([out], check=True)
