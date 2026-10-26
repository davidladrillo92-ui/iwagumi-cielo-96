#include "cielo_storage_policy.h"
#include <cassert>
#include <iostream>
using namespace cielo96;
int main(){
 ProfileImage a{};a.generation=9;a.pvCount=a.chCount=2;a.pv[1].minute=a.ch[1].minute=1440;profileSeal(a);
 auto b=a;b.generation=10;profileSeal(b);
 assert(storage::canSkipSave(a,b,true,false));
 assert(!storage::canSkipSave(a,b,false,false)&&!storage::canSkipSave(a,b,true,true));
 b.pv[63].r=92;b.ch[63].b=7;profileSeal(b);assert(storage::sameCurves(a,b));
 b.pv[0].r=1;profileSeal(b);assert(!storage::sameCurves(a,b));
 b=a;b.ch[0].r=1;profileSeal(b);assert(!storage::sameCurves(a,b));
 b=a;b.checksum^=1;assert(!storage::sameCurves(a,b));
 a.generation=0xffffffff;profileSeal(a);b=a;b.generation=0;profileSeal(b);assert(storage::sameCurves(a,b));
 std::cout<<"PASS no-op storage policy: semantic points, unused tails, first save, uncertain outcome, actual edits and generation wrap\n";
}
