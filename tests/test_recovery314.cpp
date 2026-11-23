#include "cielo_recovery314.h"
#include <assert.h>
#include <stdio.h>
using namespace cielo96::r314;
int main() {
 BootResumeInput x{};
 x.elapsed_ms=45000;x.clock_trusted=true;x.profile_valid=true;
 x.ble_ready=true;x.coexist_ready=true;x.worker_alive=true;
 x.pv_control_desired=true;x.pv_boot_off_gate=true;x.pv_off_required=true;
 x.pv_off_gatt=true;x.current_pv_day=true;
 assert(!may_resume_curves(x)); // never bypass PV OFF/recovery
 x.pv_boot_off_gate=false;x.pv_off_required=false;
 assert(may_resume_curves(x)); // safe early day without marking OTA boot healthy
 x.queue_busy=true;assert(!may_resume_curves(x));x.queue_busy=false;
 x.ch_return_pending=true;assert(!may_resume_curves(x));x.ch_return_pending=false;
 x.clock_trusted=false;assert(!may_resume_curves(x));x.clock_trusted=true;
 x.elapsed_ms=44999;assert(!may_resume_curves(x));x.elapsed_ms=45000;
 x.ota_busy=true;assert(!may_resume_curves(x));x.ota_busy=false;
 x.maintenance=true;assert(!may_resume_curves(x));x.maintenance=false;
 x.rescue=true;assert(!may_resume_curves(x));x.rescue=false;
 x.watchdog=true;assert(!may_resume_curves(x));x.watchdog=false;
 x.current_pv_day=false;x.pv_boot_off_gate=true;x.pv_off_required=true;
 x.pv_off_gatt=true;assert(may_resume_curves(x)); // night CH may resume, PV OFF holds
 x.pv_off_gatt=false;assert(!may_resume_curves(x));
 assert(may_start_ble(30000,true,true,false,false,true));
 assert(!may_start_ble(30000,false,true,false,false,true));
 assert(!may_start_ble(30000,true,false,false,false,true));
 assert(!may_start_ble(30000,true,true,true,false,true));
 assert(!may_start_ble(29999,true,true,false,false,true));
 assert(may_recover_day_pv(45000,true,true,true,true,true,true,true,false,false));
 assert(!may_recover_day_pv(45000,false,true,true,true,true,true,true,false,false));
 assert(!may_recover_day_pv(45000,true,true,true,true,true,false,true,false,false));
 assert(prioritize_pv_day(45000,true,true,true,true));
 assert(!prioritize_pv_day(90000,true,true,true,true));
 assert(trusted_rtc_continuity(true,1795454000,1795454001,true));
 assert(!trusted_rtc_continuity(false,1795454000,1795454001,true));
 assert(!trusted_rtc_continuity(true,1795454000,1795454901,true));
 assert(!trusted_rtc_continuity(true,1795454000,1795453800,true));
 assert(!trusted_rtc_continuity(true,1795454000,1795454001,false));
 assert(matching_return(7,7,true));
 assert(!matching_return(7,6,true));
 assert(!matching_return(7,7,false));
 assert(!matching_return(0,0,true));
 assert(cold_discovery_slow(5785,5001));
 assert(!cold_discovery_slow(6722,0));
 puts("OK 3.14: fast boot interlocks, offline BLE, no untrusted clock, request IDs");
}
