"""Execute the actual commit handler; identical input must never call storage."""
from pathlib import Path
import tempfile,subprocess,json
s=Path('src/main.cpp').read_text()
positive=s[s.index('static bool positiveInt('):s.index('static bool parseBoundedUInt(')]
parsers=s[s.index('static bool parsePoints('):s.index('static void saveBothCurves(){')]
handler=s[s.index('static void saveBothCurves(){'):s.index('static String cadenceJson(){')]
fixture=r'''
#include "cielo_storage_policy.h"
#include "cielo_profile_transaction.h"
#include <string>
#include <map>
#include <cassert>
#include <iostream>
#include <type_traits>
#include <cctype>
struct String {
 std::string v;String()=default;String(const char*s):v(s){}String(const std::string&s):v(s){}
 template<class T,typename std::enable_if<std::is_integral<T>::value,int>::type=0>String(T n):v(std::to_string(n)){}
 size_t length()const{return v.size();}bool isEmpty()const{return v.empty();}const char* c_str()const{return v.c_str();}
 char operator[](size_t i)const{return v[i];}size_t indexOf(char c,size_t at=0)const{return v.find(c,at);}
 String substring(size_t a,size_t b)const{return v.substr(a,b-a);}
 bool equalsIgnoreCase(const String& b)const{if(v.size()!=b.v.size())return false;for(size_t i=0;i<v.size();i++)if(std::tolower((unsigned char)v[i])!=std::tolower((unsigned char)b.v[i]))return false;return true;}
 bool operator!=(const char*s)const{return v!=s;}
 String& operator+=(const String& s){v+=s.v;return *this;}friend String operator+(String a,const String& b){return a+=b;}
};
String jsonQuote(const String&s){return String("\"")+s+String("\"");}
struct {std::map<std::string,String> args;String arg(const char*k){return args[k];}}srv;
cielo96::ProfileImage profile;const char* loadedCurveBackend="NVS_LEGACY";
bool profileRecovery=false,lastCurveRequestUnchanged=false,lastPvDesired=true,lastChDesired=true;
uint32_t curveUnchangedSavesSkipped=0,writes=0;bool accept=true;
cielo96::profiletxn::Result lastCurveTransaction;
cielo96::nvssafe::Failure lastCurveNvsFailure=cielo96::nvssafe::Failure::None;
constexpr int ESP_ERR_NVS_NOT_ENOUGH_SPACE=4357;int lastCurveNvsErr=0;
const char* lastCurveNvsStage="sin_intento";const char* esp_err_to_name(int){return "ESP_OK";}
String lastBackupError="ninguno";struct {bool published(){return false;}bool uncertain(){return false;}}curveFs;
int status=0;String body;void fail(int code,const char*s){status=code;body=s;}
void jsonSend(int code,const String&s){status=code;body=s;}
void record(const char*,const String&){}
bool saveProfile(const cielo96::ProfileImage& p){writes++;if(accept)profile=p;return accept;}
void setup(){
 profile=cielo96::ProfileImage{};profile.pvCount=profile.chCount=2;profile.pv[1].minute=profile.ch[1].minute=1440;
 profile.generation=9;cielo96::profileSeal(profile);
 srv.args["schema_version"]="2";srv.args["generation"]="9";
 srv.args["pv"]="0,0,0,0,0;1440,0,0,0,0";srv.args["ch"]="0,0,0,0;1440,0,0,0";
 writes=0;curveUnchangedSavesSkipped=0;accept=true;loadedCurveBackend="NVS_LEGACY";
 lastCurveTransaction={};lastPvDesired=lastChDesired=true;
}
void checksum(){char b[9];const auto& pv=srv.args["pv"];const auto& ch=srv.args["ch"];
 snprintf(b,sizeof(b),"%08lx",(unsigned long)cielo96::curves::wireHash(pv.c_str(),pv.length(),ch.c_str(),ch.length()));srv.args["checksum"]=b;}
'''
checks=r'''
int main(){
 setup();checksum();auto saved=profile;
 saveBothCurves();assert(status==200&&writes==0&&curveUnchangedSavesSkipped==1&&lastCurveRequestUnchanged);
 assert(memcmp(&saved,&profile,sizeof(profile))==0&&lastPvDesired&&lastChDesired);
 std::cout<<body.v<<"\n";
 saveBothCurves();assert(writes==0&&profile.generation==9&&curveUnchangedSavesSkipped==2);
 srv.args["generation"]="8";saveBothCurves();assert(status==409&&writes==0);
 setup();srv.args["checksum"]="00000000";saveBothCurves();assert(status==400&&writes==0);
 setup();checksum();loadedCurveBackend="DEFAULT_RAM";saveBothCurves();assert(writes==1&&profile.generation==10);
 setup();checksum();lastCurveTransaction.writeMayHavePersisted=true;accept=false;saveBothCurves();assert(status==500&&writes==1&&!lastCurveRequestUnchanged);
 setup();srv.args["pv"]="0,1,0,0,0;1440,0,0,0,0";checksum();saveBothCurves();
 assert(status==200&&writes==1&&profile.generation==10&&!lastPvDesired&&!lastChDesired);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.cpp';p.write_text(fixture+positive+parsers+handler+checks);out=str(Path(d)/'test')
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-Iinclude',str(p),'-o',out],check=True)
 r=json.loads(subprocess.check_output([out],text=True));assert r['unchanged'] and r['generation']==9 and r['pv']==r['chi']==2
print('PASS actual commit API: repeated identical curves write zero bytes, generation/cache unchanged, stale/integrity errors, first persistent save, uncertain result and real edit')
