#include "cielo_clock_evidence.h"
#include <cassert>
int main(){
 using namespace cielo96::clock_evidence;
 const uint32_t actual=1792965996UL; // 2026-10-25, trusted after phone sync
 const uint32_t phantom=1867341431UL; // wrong boot epoch (2029)
 assert(!trusted_this_boot(false,false,phantom)); // plausible year is not proof
 assert(!trusted_this_boot(false,false,actual));
 assert(trusted_this_boot(true,false,actual));
 assert(trusted_this_boot(false,true,actual));
 assert(!trusted_this_boot(true,false,0));
 assert(implausible_boot_epoch(phantom,actual,279));
 assert(implausible_boot_epoch(0,actual,279));
 assert(!implausible_boot_epoch(actual-279,actual,279));
 assert(implausible_boot_epoch(actual-5000,actual,279));
 return 0;
}
