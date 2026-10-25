#include "cielo_pv_day_recovery.h"
#include "cielo_ble_selective.h"
#include "cielo_ble_blockage.h"
#include <cassert>
int main(){
 cielo96::pvday::Input d{true,false,true,true,true,true,false,true,45};
 assert(!cielo96::pvday::can_resume(d));
 d.off_gatt=true;assert(cielo96::pvday::can_resume(d));
 d.current_brightness=0;assert(!cielo96::pvday::can_resume(d));
 d.current_brightness=45;d.clock_ok=false;assert(!cielo96::pvday::can_resume(d));
 assert(cielo96::pvday::defer_ch_return(true,19999));
 assert(!cielo96::pvday::defer_ch_return(true,20000));
 cielo96::selective::Input s{true,true,false,120000,1000,1000,255,false,false};
 assert(cielo96::selective::hold_ch_only(s));
 s.ch_pending=false;assert(!cielo96::selective::hold_ch_only(s));
 s.ch_pending=true;s.ch_pending_age_ms=300000;assert(!cielo96::selective::hold_ch_only(s));
 s.ch_pending_age_ms=120000;s.worker_heartbeat_age_ms=120000;
 assert(!cielo96::selective::hold_ch_only(s));
 s.worker_heartbeat_age_ms=1000;s.pv_off_required=true;
 assert(!cielo96::selective::hold_ch_only(s));
 auto pending=cielo96::blockage::classify({2,255,1000,1000,0,120000});
 assert(pending.primary==cielo96::blockage::Kind::SdkConnectPending);
 auto gatt=cielo96::blockage::classify({5,1,120000,120000,0,0});
 assert(gatt.primary==cielo96::blockage::Kind::GattDiscoveryWait);
}
