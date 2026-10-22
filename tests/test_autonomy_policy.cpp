#include "cielo_autonomy_policy.h"
#include <cassert>
using namespace cielo96::autonomy;
int main(){
 assert(enabled(false,false));assert(enabled(false,true));
 assert(enabled(true,true));assert(!enabled(true,false));
 Retry r;r.arm(100);assert(r.pending&&r.episodes==1&&r.due==30100);
 r.arm(500);assert(r.due==30100&&r.episodes==1);
 assert(!r.ready(30099,true));assert(!r.ready(30100,false));assert(r.ready(30100,true));
 r.begin();assert(!r.pending&&r.recovering&&r.attempts==1);r.arm(30101);assert(r.due==90101);
 r.begin();r.arm(90102);assert(r.due==210102);
 r.begin();r.arm(210103);assert(r.due==450103);
 r.begin();r.arm(450104);assert(r.due==750104);
 r.success();assert(!r.pending&&!r.recovering&&r.episodes==0);
 r.arm(0xfffffff0UL);assert(!r.ready(100,true));assert(r.ready(29984,true));
 r.cancel();assert(!r.pending&&!r.ready(40000,true));
}
