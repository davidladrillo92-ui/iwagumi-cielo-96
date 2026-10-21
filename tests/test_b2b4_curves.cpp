#include "cielo_curve_policy.h"
#include "cielo_ble_queue.h"
#include <cassert>
#include <string>
#include <cstring>
using namespace cielo;
using namespace cielo96;
int main(){
 // 64 puntos en ambos canales, sin modificar RGB almacenados.
 ProfileImage v{};v.pvCount=64;v.chCount=64;
 for(unsigned i=0;i<64;i++){
  const uint16_t m=static_cast<uint16_t>(i==63?1440:i*22);
  v.pv[i]={m,static_cast<uint8_t>(i*4),static_cast<uint8_t>(i),static_cast<uint8_t>(i),static_cast<uint8_t>(i==0||i==63?0:20)};
  v.ch[i]={m,static_cast<uint8_t>(i==63?0:i),static_cast<uint8_t>(i==63?0:i),static_cast<uint8_t>(i==63?0:i)};
 }
 profileSeal(v);assert(curves::curveValid(v));
 auto a=at(v.pv,v.pvCount,440);auto b=atSecond(v.pv,v.pvCount,440*60);
 assert(a.r==b.r && a.g==b.g && a.b==b.b && a.brightness==b.brightness);
 auto mid=atSecond(v.pv,v.pvCount,440*60+15);
 assert(mid.r>=a.r);
 // Max 64, 65 es incompatible con la imagen y el parser.
 v.pvCount=65;profileSeal(v);assert(!curves::curveValid(v));v.pvCount=64;profileSeal(v);
 // Simulación: corte de alimentación a mitad de escritura del slot inactivo.
 ProfileImage old=v;old.generation=8;profileSeal(old);
 ProfileImage candidate=old;candidate.generation=9;candidate.pv[5].r=202;candidate.ch[5].g=85;profileSeal(candidate);
 assert(curves::curveValid(old)&&curves::curveValid(candidate));
 // NVS válido antiguo + slot nuevo truncado => el antiguo sigue disponible.
 auto interrupted=candidate;interrupted.checksum^=0x12345678;
 assert(!curves::curveValid(interrupted));
 auto chosen=curves::newestValid(&old,&interrupted);
 assert(chosen==&old && chosen->pv[5].r==old.pv[5].r && chosen->ch[5].g==old.ch[5].g);
 // Si la escritura fue completamente verificada, ambos canales avanzan juntos.
 chosen=curves::newestValid(&old,&candidate);
 assert(chosen==&candidate && chosen->pv[5].r==202 && chosen->ch[5].g==85);
 assert(!curves::expectedGeneration(10,9));
 // Una escritura posterior tras generacion 9 debe usar el slot contrario.
 assert(curves::expectedGeneration(9,9));
 // Integridad del payload ASCII: 1 byte modificado debe cambiar checksum.
 std::string pv="870,255,120,20,0;1440,0,0,0,0",ch="870,70,65,60;1440,0,0,0";
 const auto checksum=curves::wireHash(pv.data(),pv.size(),ch.data(),ch.size());
 pv[5]='1';assert(checksum!=curves::wireHash(pv.data(),pv.size(),ch.data(),ch.size()));
 // Carga V5: salto luna directo a 23:00 sin interpolacion malva 22:59:15, :30, :45.
 const ChiPoint moon[]={{1378,3,3,3},{1379,2,2,2},{1380,1,1,2},{1439,1,1,2},{1440,0,0,0}};
 for(int sec=0;sec<60;sec+=15){auto c=atSecond(moon,5,1379UL*60+sec);assert(c.r==2&&c.g==2&&c.b==2);}
 auto c=atSecond(moon,5,1380UL*60);assert(c.r==1&&c.g==1&&c.b==2);
 auto d=atSecond(moon,5,1390UL*60);assert(d.r==1&&d.g==1&&d.b==2);
 // Ensayo 15s no se activa con fallos, saturacion BLE, memoria <80KB o OTA.
 assert(curves::secondsModeSafe(true,true,true,false,140000,0));
 assert(!curves::secondsModeSafe(true,true,true,false,79000,0));
 assert(!curves::secondsModeSafe(true,true,true,false,140000,3));
 assert(!curves::secondsModeSafe(true,true,false,false,140000,0));
 // No lanzar dos comandos para la misma pantalla; se sustituyen los pendientes.
 ble::Scheduler q;
 assert(q.submit(ble::Device::PezVerde,ble::Action::Apply,5,5,5,12,100));
 assert(q.submit(ble::Device::PezVerde,ble::Action::Apply,7,7,7,14,110));
 ble::Command cmd;assert(q.start_next(120,cmd));assert(cmd.r==7&&cmd.brightness==14);
}
