#include "cielo_monitor_service.h"
#include <esp_system.h>
#include <mbedtls/sha256.h>
namespace cielo96 { namespace monitor { namespace {
WebServer* server=nullptr;Preferences* storage=nullptr;
bool (*adminGuard)()=nullptr;bool (*privateGuard)()=nullptr;Snapshot (*sample)()=nullptr;
uint8_t credentialHash[32]{};bool enabled=false,served=false;uint32_t lastPoll=0;
char buffer[JSON_CAPACITY];
bool digest(const String&s,uint8_t*out){return mbedtls_sha256_ret((const uint8_t*)s.c_str(),s.length(),out,0)==0;}
bool equal(const uint8_t*a,const uint8_t*b){uint8_t d=0;for(size_t i=0;i<32;i++)d|=a[i]^b[i];return d==0;}
void reply(int status,const String&s){server->sendHeader("Cache-Control","no-store");server->send(status,"application/json",s);}
bool tls(){if(server->client().remoteIP()==IPAddress(127,0,0,1))return true;reply(426,"{\"error\":\"HTTPS requerido\"}");return false;}
void rotate(){
 if(!adminGuard())return;
 if(sample().ota){reply(423,"{\"error\":\"Espera a terminar OTA\"}");return;}
 uint8_t random[32],hash[32];esp_fill_random(random,sizeof(random));
 char token[65];const char*hex="0123456789abcdef";
 for(size_t i=0;i<32;i++){token[2*i]=hex[random[i]>>4];token[2*i+1]=hex[random[i]&15];}token[64]=0;
 if(!digest(String(token),hash)||storage->putBytes("mon_hash",hash,32)!=32){
  memset(token,0,sizeof(token));memset(random,0,sizeof(random));reply(507,"{\"error\":\"Credencial no guardada\"}");return;
 }
 memcpy(credentialHash,hash,32);enabled=true;
 reply(200,String("{\"token\":\"")+token+"\",\"scope\":\"monitor:read\"}");
 memset(token,0,sizeof(token));memset(random,0,sizeof(random));
}
void disable(){
 if(!adminGuard())return;
 if(sample().ota){reply(423,"{\"error\":\"Espera a terminar OTA\"}");return;}
 if(storage->isKey("mon_hash")&&!storage->remove("mon_hash")){reply(507,"{\"error\":\"No se pudo revocar\"}");return;}
 enabled=false;memset(credentialHash,0,sizeof(credentialHash));reply(200,"{\"ok\":true}");
}
void read(){
 if(!tls())return;
 if(!enabled){reply(403,"{\"error\":\"Seguimiento desactivado\"}");return;}
 const String token=server->header("X-Cielo-Monitor");uint8_t hash[32];
 if(token.length()!=64||!digest(token,hash)||!equal(hash,credentialHash)){reply(401,"{\"error\":\"Acceso de lectura rechazado\"}");return;}
 const uint32_t now=millis();
 if(served&&now-lastPoll<15000UL){reply(429,"{\"error\":\"Consulta cada 15 segundos como maximo\"}");return;}
 const Snapshot m=sample();
 if(m.ota){reply(503,"{\"error\":\"OTA en curso\"}");return;}
 if(!encode(m,buffer,sizeof(buffer))){reply(500,"{\"error\":\"Limite de telemetria\"}");return;}
 lastPoll=now;served=true;reply(200,buffer);
}
} // namespace
void attach(WebServer&s,Preferences&p,bool(*admin)(),bool(*privateRead)(),Snapshot(*snapshot)()){
 server=&s;storage=&p;adminGuard=admin;privateGuard=privateRead;sample=snapshot;
 enabled=p.getBytesLength("mon_hash")==32&&p.getBytes("mon_hash",credentialHash,32)==32;
 s.on("/api/monitor",HTTP_GET,read);
 s.on("/api/monitor/token",HTTP_POST,rotate);
 s.on("/api/monitor/disable",HTTP_POST,disable);
 s.on("/api/monitor/status",HTTP_GET,[]{if(privateGuard())reply(200,
  String("{\"enabled\":")+(enabled?"true":"false")+",\"observer\":\"EXTERNAL_REQUIRED\"}");});
}
} }
