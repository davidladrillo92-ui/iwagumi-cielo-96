#include "cielo_pv_day_recovery.h"
#include "cielo_ble_selective.h"
#include "cielo_ble_blockage.h"
#include <cassert>
int main(){
 // Early daytime recovery: never during OTA or without Wi-Fi/coexistence.
 assert(!cielo96::pvday::early_boot_ready(true,false,44999,true,true,false,false));
 assert(cielo96::pvday::early_boot_ready(true,false,45000,true,true,false,false));
 assert(!cielo96::pvday::early_boot_ready(true,true,180000,true,true,false,false));
 assert(!cielo96::pvday::early_boot_ready(true,false,60000,false,true,false,false));
 assert(!cielo96::pvday::early_boot_ready(true,false,60000,true,false,false,false));
 assert(!cielo96::pvday::early_boot_ready(true,false,60000,true,true,true,false));
 assert(!cielo96::pvday::early_boot_ready(true,false,60000,true,true,false,true));
 assert(!cielo96::pvday::early_boot_ready(false,false,60000,true,true,false,false));
 cielo96::pvday::Input d{true,false,true,true,true,true,false,true,45};
 assert(!cielo96::pvday::can_resume(d));
 d.off_gatt=true;assert(cielo96::pvday::can_resume(d));
 d.current_brightness=0;assert(!cielo96::pvday::can_resume(d));
 d.current_brightness=45;d.clock_ok=false;assert(!cielo96::pvday::can_resume(d));
 // Chihiros must not seize BLE after the first PV OFF GATT at ~37s.
 assert(cielo96::pvday::prioritize_boot_day(true,true,true,true,false,37000));
 assert(cielo96::pvday::prioritize_boot_day(true,true,true,true,false,44999));
 assert(!cielo96::pvday::prioritize_boot_day(true,true,true,true,false,65000));
 assert(!cielo96::pvday::prioritize_boot_day(true,true,true,false,false,37000));
 assert(!cielo96::pvday::prioritize_boot_day(true,true,false,true,false,37000));
 assert(!cielo96::pvday::prioritize_boot_day(true,true,true,true,true,37000));
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
