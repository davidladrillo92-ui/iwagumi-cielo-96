#include "cielo_monitor_service.h"
namespace cielo96 { namespace monitor { namespace {
WebServer* server=nullptr;Preferences* storage=nullptr;
bool (*adminGuard)()=nullptr;bool (*privateGuard)()=nullptr;Snapshot (*sample)()=nullptr;
bool enabled=true,served=false;uint32_t lastPoll=0;
char buffer[JSON_CAPACITY];
void reply(int status,const String&s){server->sendHeader("Cache-Control","no-store");server->send(status,"application/json",s);}
void setEnabled(bool value){
 if(!adminGuard())return;
 if(sample().ota){reply(423,"{\"error\":\"Espera a terminar OTA\"}");return;}
 if(storage->putBool("mon_on",value)!=1){reply(507,"{\"error\":\"No se pudo guardar\"}");return;}
 enabled=value;reply(200,"{\"ok\":true}");
}
void read(){
 if(!privateGuard())return;
 if(!enabled){reply(403,"{\"error\":\"Seguimiento desactivado\"}");return;}
 const uint32_t now=millis();
 if(served&&now-lastPoll<15000UL){reply(429,"{\"error\":\"Consulta cada 15 segundos como maximo\"}");return;}
 const Snapshot m=sample();
 if(m.ota){reply(503,"{\"error\":\"OTA en curso\"}");return;}
 if(!encode(m,buffer,sizeof(buffer))){reply(500,"{\"error\":\"Limite de telemetria\"}");return;}
 lastPoll=now;served=true;reply(200,buffer);
}
}
void attach(WebServer&s,Preferences&p,bool(*admin)(),bool(*privateRead)(),Snapshot(*snapshot)()){
 server=&s;storage=&p;adminGuard=admin;privateGuard=privateRead;sample=snapshot;
 enabled=p.getBool("mon_on",true);
 s.on("/api/monitor",HTTP_GET,read);
 s.on("/api/monitor/enable",HTTP_POST,[]{setEnabled(true);});
 s.on("/api/monitor/disable",HTTP_POST,[]{setEnabled(false);});
 s.on("/api/monitor/status",HTTP_GET,[]{if(privateGuard())reply(200,
  String("{\"enabled\":")+(enabled?"true":"false")+",\"access\":\"LAN_NO_PASSWORD\",\"last_poll_age_sec\":"+
  (served?String((millis()-lastPoll)/1000UL):String("null"))+",\"observer\":\"EXTERNAL_REQUIRED\"}");});
}
} }
