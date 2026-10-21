#include "cielo_ble_queue.h"
#include "cielo_codec.h"
#include "cielo_night_safety.h"
#include <cassert>
#include <cstdint>
using cielo::ble::Scheduler;
using cielo::ble::Action;
using cielo::ble::Device;
using cielo::ble::Command;
using namespace cielo96::safety;
int main(){
 // 23:59, cola de luz de dia, se pierde conexion BLE antes del cero.
 Scheduler q(3500,250,2000);
 assert(q.submit(Device::PezVerde,Action::Apply,255,160,120,24,1000));
 // 00:00: borrar RGB antiguo y dar prioridad a POWER OFF fisico.
 assert(q.cancel_pending(Device::PezVerde));
 assert(offRequired(true,true,true,0));
 assert(q.submit(Device::PezVerde,Action::Apply,0,0,0,0,2000));
 Command cmd{};
 assert(q.start_next(2000,cmd));
 assert(cmd.brightness==0);
 uint8_t frame[9];cielo96::codec::pv_power(false,frame);
 const uint8_t powerOff[9]={0x7e,0x07,0x04,0,0,0,0x02,0x01,0xef};
 for(int i=0;i<9;++i)assert(frame[i]==powerOff[i]);
 const uint32_t first=cmd.sequence;
 // Primer fallo GATT: no se debe declarar apagado.
 assert(q.complete(first,false,4000));
 assert(!isOffAck(true,first,first+1));
 assert(q.start_next(6500,cmd));assert(cmd.brightness==0);
 assert(q.complete(cmd.sequence,false,7000));
 // Tercer fallo (la libreria reintenta y luego puede agotar la orden).
 q.start_next(15000,cmd);
 assert(cmd.brightness==0);
 assert(q.complete(cmd.sequence,false,16000));
 // 00:05: la conexion regresa. El supervisor EXTERNO A LA COLA
 // mantiene la obligacion y encola una orden nueva con datos actuales.
 assert(offRequired(true,true,true,0));
 q.cancel_pending(Device::PezVerde);
 assert(q.submit(Device::PezVerde,Action::Apply,0,0,0,0,300000));
 assert(q.start_next(300001,cmd));assert(cmd.brightness==0);
 const uint32_t reconnected=cmd.sequence;
 assert(q.complete(reconnected,true,300500));
 assert(isOffAck(true,reconnected,reconnected));
 // Un GATT correcto no constituye prueba optica.
 assert(!isOffAck(true,reconnected,reconnected+1));
 // Retorno de alimentacion: con reloj ausente NO adivinar la hora.
 assert(!offRequired(true,false,true,0));
 // Solo al recuperar el reloj fiable se reemite el apagado.
 assert(offRequired(true,true,true,0));
 // No admitir colores antiguos aunque hubiese cambiado la cola.
 assert(forbidStaleRgb(true,17));assert(!forbidStaleRgb(true,0));
 // La politica no codifica AUTO de escenas de Chihiros.
 uint16_t id=1;uint8_t payload[32];size_t len=0;
 assert(cielo96::codec::ch_custom_curve_resume(id,payload,sizeof(payload),len));
 assert(payload[5]==0x05 && payload[6]==3);
 return 0;
}
