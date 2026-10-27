#include "cielo_wifi_config.h"
#include <cassert>
#include <map>
#include <string>
#include <iostream>
using namespace cielo96::wificfg;
struct Store{
 static constexpr int Ok=0,Missing=1,Corrupt=-1,NoSpace=2;
 std::map<std::string,Image> data;int writes=0,error=0;
 int read(const char*k,Image&i){if(error)return error;auto it=data.find(k);if(it==data.end())return Missing;i=it->second;return Ok;}
 int write(const char*k,const Image&i){assert(strcmp(k,"wifiA")==0||strcmp(k,"wifiB")==0);data[k]=i;writes++;return Ok;}
 int commit(){return Ok;}
};
Image sample(uint32_t gen=1,const char*ssid="home"){Image i;assert(make(i,ssid,strlen(ssid),"password",8,true));i.generation=gen;seal(i);return i;}
int main(){
 Image i;assert(make(i,"",0,"",0,false)&&valid(i));assert(!make(i,"a",1,"b",1,true));
 std::string ssid(32,'s'),pass(63,'p');assert(make(i,ssid.c_str(),32,pass.c_str(),63,true)&&valid(i));
 assert(!make(i,ssid.c_str(),33,pass.c_str(),63,true));assert(!make(i,"s",1,pass.c_str(),64,true));
 assert(!make(i,"s\n",2,"password",8,true));assert(!make(i,"s",1,"pass\0ord",8,true));
 assert(make(i,"s",1,"",0,true));i.ssid[31]=1;seal(i);assert(!valid(i));
 i=sample();i.password[63]=1;seal(i);assert(!valid(i));i=sample();i.checksum^=1;assert(!valid(i));
 Store s;assert(inspect(s).origin==Origin::Legacy);s.data["wifiA"]=sample(0xffffffff);s.data["wifiB"]=sample(0);
 assert(inspect(s).active==1);s.data["wifiB"]=sample(0x7fffffff);assert(inspect(s).origin==Origin::Blocked);
 s.data["wifiB"]=sample(0xffffffff,"other");assert(inspect(s).origin==Origin::Blocked);
 s.data["wifiB"]=s.data["wifiA"];assert(inspect(s).active==0);
 s.data["wifiB"].schema=2;seal(s.data["wifiB"]);assert(inspect(s).origin==Origin::Blocked);
 s.data.erase("wifiB");s.error=-9;assert(inspect(s).origin==Origin::Blocked);s.error=0;
 s.data["wifiA"].checksum^=1;assert(inspect(s).origin==Origin::Blocked);
 s.data.clear();Image out;auto r=save(s,sample(),out);assert(r.verified&&r.generation==1);
 Image disabled;assert(make(disabled,"",0,"",0,false));r=save(s,disabled,out);assert(r.verified&&r.generation==2);
 auto boot=inspect(s);assert(boot.origin==Origin::Atomic&&!boot.image.enabled);
 const int writes=s.writes;r=save(s,disabled,out);assert(r.verified&&r.unchanged&&s.writes==writes);
 std::cout<<"PASS Wi-Fi record: limits, checksum, schema, wrap/conflicts, read failures, disabled tombstone and zero-write no-op\n";
}
