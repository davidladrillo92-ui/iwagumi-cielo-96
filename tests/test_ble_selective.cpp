#include "cielo_ble_selective.h"
#include <cassert>
using namespace cielo96::selective;
int main(){
 Input x{true,true,false,120000,1000,1000,255,false,false};
 assert(hold_ch_only(x)); // CH SDK stuck, BLE worker still alive
 x.worker_device=0;assert(hold_ch_only(x)); // PV can still run
 x.pv_off_required=true;assert(!hold_ch_only(x));x.pv_off_required=false;
 x.pv_pending=true;assert(!hold_ch_only(x));x.pv_pending=false;
 x.worker_heartbeat_age_ms=16000;assert(!hold_ch_only(x));x.worker_heartbeat_age_ms=1000;
 x.worker_phase_age_ms=16000;assert(!hold_ch_only(x));x.worker_phase_age_ms=1000;
 x.ch_pending_age_ms=300000;assert(!hold_ch_only(x));
 x.ch_pending_age_ms=14999;assert(!hold_ch_only(x));
 x.ch_pending_age_ms=15000;assert(hold_ch_only(x));
 x.ch_pending=false;assert(!hold_ch_only(x));x.ch_pending=true;
 x.ota_or_rescue=true;assert(!hold_ch_only(x));x.ota_or_rescue=false;
 x.ble_ready=false;assert(!hold_ch_only(x));
 return 0;
}
