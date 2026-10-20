#include "cielo_ble_queue.h"
#include <assert.h>
#include <stdio.h>
int main(){
 using namespace cielo::ble;
 Scheduler q(100,20,1000);Command cmd{};
 assert(q.submit(Device::PezVerde,Action::Apply,200,0,0,30,0,1000));
 assert(q.submit(Device::PezVerde,Action::Apply,210,0,0,40,1,1000));
 assert(q.metrics().superseded==1);
 assert(q.start_next(2,cmd));assert(cmd.r==210);
 q.tick(110);assert(q.busy()&&q.stalled()); // soft timeout does not release radio
 assert(!q.start_next(111,cmd));
 assert(q.complete(cmd.sequence,true,112));assert(!q.busy());
 assert(q.submit(Device::Chihiros,Action::RestoreAuto,0,0,0,0,120));
 assert(!q.submit(Device::Chihiros,Action::Apply,10,10,10,0,121));
 assert(q.start_next(122,cmd));assert(cmd.action==Action::RestoreAuto);
 assert(q.complete(cmd.sequence,true,130));
 // El selector My Chihiros no puede verse anulado por un RGB viejo en cola.
 Scheduler r(200,20,1000);Command x{};
 assert(r.submit(Device::Chihiros,Action::Apply,30,30,30,0,500,90000));
 assert(r.cancel_pending(Device::Chihiros));
 assert(!r.pending(Device::Chihiros));
 assert(!r.start_next(501,x));
 assert(!r.cancel_pending(Device::Chihiros));
 // Si ya comenzo un envio, no se corta el trabajador BLE a mitad de trama:
 // solo se retiran los siguientes comandos; despues se recupera la curva.
 assert(r.submit(Device::Chihiros,Action::Apply,70,65,60,0,502));
 assert(r.start_next(503,x));
 assert(r.submit(Device::PezVerde,Action::Apply,240,238,230,20,504));
 assert(!r.cancel_pending(Device::Chihiros));
 assert(r.complete(x.sequence,true,505));
 assert(r.start_next(525,x));assert(x.device==Device::PezVerde);
 assert(r.complete(x.sequence,true,526));
 puts("OK: dedup, BLE serializado, timeout blando, prioridad y cancelacion segura antes del retorno");
}
