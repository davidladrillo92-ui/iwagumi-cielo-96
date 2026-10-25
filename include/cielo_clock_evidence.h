#pragma once
#include <stdint.h>
namespace cielo96 { namespace clock_evidence {
// The RTC can contain a plausible-looking but wrong year after a reboot.
// Diagnostics may timestamp events only after a fresh NTP or phone sync.
inline bool trusted_this_boot(bool ntp_sync,bool phone_sync,uint32_t epoch){
 return (ntp_sync||phone_sync) && epoch>1700000000UL;
}
inline bool implausible_boot_epoch(uint32_t stored,uint32_t trusted_now,uint32_t uptime_sec){
 if(!stored)return true;
 if(stored>trusted_now+300UL)return true;
 const uint32_t oldest=trusted_now>uptime_sec+3600UL?trusted_now-uptime_sec-3600UL:0;
 return stored<oldest;
}
} }
