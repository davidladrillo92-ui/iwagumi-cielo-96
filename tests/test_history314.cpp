#include "cielo_history314.h"
#include <assert.h>
#include <stdio.h>
using namespace cielo96::history314;
int main(){
 Ring r;
 for(uint32_t d=1;d<=35;d++)r.sample(2026000+d,1,2,0,0,100000-d);
 assert(r.count()==30);
 assert(r.oldest(0).day_key==2026006);
 assert(r.oldest(29).day_key==2026035);
 r.sample(2026035,4,7,1,2,90000);
 assert(r.oldest(29).samples==2);
 assert(r.oldest(29).pv_ok==5);
 assert(r.oldest(29).ch_ok==9);
 assert(r.oldest(29).heap_min==90000);
 r.sample(2026034,99,99,9,9,1); // backwards wall clock is ignored
 assert(r.oldest(29).pv_ok==5);
 r.sample(2026036,0,0,0,0,0); // invalid heap value ignored
 assert(r.oldest(29).day_key==2026035);
 puts("OK 3.14: 7/30 day bounded RAM history; no flash or NVS needed");
}
