#include "cielo_ota_validation.h"
#include <cassert>
#include <cstring>
#include <initializer_list>
using namespace cielo96::ota;
int main(){
 HealthyWindow window;
 // Every 10ms loop tick must preserve the same start, instead of resetting it.
 for(uint32_t now=0;now<15000;now+=10)assert(!window.tick(now,true));
 assert(window.tick(15000,true));
 assert(!window.tick(15001,false));assert(!window.tick(15002,true));
 assert(!window.tick(30001,true));assert(window.tick(30002,true));
 window={};assert(!window.tick(0xfffffff0UL,true));
 assert(!window.tick(100,true));assert(window.tick(14984,true));
 uint8_t p[prefixBytes]{};
 p[0]=0xe9;p[1]=5;p[3]=0x3f;p[12]=9;
 p[32]=0x32;p[33]=0x54;p[34]=0xcd;p[35]=0xab;
 assert(compatiblePrefix(p,sizeof(p)));
 assert(!compatiblePrefix(nullptr,0));assert(!compatiblePrefix(p,287));
 for(unsigned offset:{0U,1U,3U,12U,13U,32U,33U,34U,35U}){
  auto old=p[offset];p[offset]=0xff;assert(!compatiblePrefix(p,sizeof(p)));p[offset]=old;
 }
 return 0;
}
