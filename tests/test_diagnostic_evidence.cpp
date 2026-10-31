#include "cielo_diagnostic_evidence.h"
#include <cassert>
#include <string>
using namespace cielo96::evidence;
int main(){
 assert(windowSamples(0)==0&&windowSamples(10)==10&&windowSamples(3127)==32);
 assert(basisPoints(355,11694)==303&&basisPoints(110,5883)==186);
 assert(basisPoints(0,0)==0&&basisPoints(100,1)==10000);
 assert(saveResult(false,false,false,false,false)==Save::NotAttempted);
 assert(saveResult(false,true,false,false,false)==Save::Unchanged);
 assert(saveResult(true,false,true,false,false)==Save::Verified);
 assert(saveResult(true,false,true,true,false)==Save::Failed);
 assert(saveResult(true,false,true,true,true)==Save::Uncertain);
 OperationalHistory h;
 h.capture("MANTENIMIENTO_COMPROBANDO",10,0);
 h.capture("MANTENIMIENTO_INICIO",20,100);
 for(int i=0;i<1000;i++)h.capture("PV_POWER_OFF_GATT_OK",30+i,200);
 h.capture("MANTENIMIENTO_FIN",1040,300);
 assert(h.count==3&&h.at(0).epoch==0&&h.at(2).uptime_ms==1040);
 assert(std::string(eventName(h.at(0).code))=="MANTENIMIENTO_COMPROBANDO");
 for(int i=0;i<20;i++)h.capture("CURVA_GUARDADA_FS",2000+i,400);
 assert(h.count==16&&h.at(0).uptime_ms==2004&&h.at(15).uptime_ms==2019);
 assert(eventCode("arbitrary private SSID or password")==0&&eventCode(nullptr)==0);
 cielo96::ProfileImage p{};p.pvCount=2;p.chCount=2;
 p.pv[0]={839,10,22,62,0};p.pv[1]={1440,58,78,132,0};
 p.ch[0]={870,1,1,1};p.ch[1]={1440,0,0,0};
 const char* pv="839,10,22,62,0;1440,58,78,132,0";const char* ch="870,1,1,1;1440,0,0,0";
 assert(wireChecksum(p)==cielo96::curves::wireHash(pv,strlen(pv),ch,strlen(ch)));
 const auto sum=wireChecksum(p);p.generation++;assert(wireChecksum(p)==sum);
 p.ch[0].b++;assert(wireChecksum(p)!=sum);
 return 0;
}
