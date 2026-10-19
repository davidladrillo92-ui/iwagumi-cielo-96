#include "cielo_codec.h"
#include "cielo_profile_store.h"
#include "profiles_generated.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(){
 using namespace cielo96;
 uint8_t b[40]{};
 codec::pv_color(0x14,0,0,b);
 const uint8_t red[9]={0x7e,0x07,0x05,0x03,0x14,0,0,0x10,0xef};
 assert(memcmp(b,red,9)==0);
 codec::pv_power(false,b);
 const uint8_t off[9]={0x7e,0x07,0x04,0x00,0,0,0x02,0x01,0xef};
 assert(memcmp(b,off,9)==0);
 codec::pv_brightness(30,b);
 const uint8_t br[9]={0x7e,0x04,0x01,30,0x01,0xff,0x02,0x01,0xef};
 assert(memcmp(b,br,9)==0);
 uint16_t id=0x001f;size_t len=0;
 assert(codec::ch_manual(0,100,id,b,sizeof(b),len));
 const uint8_t example[9]={0x5a,0x01,0x07,0x00,0x20,0x07,0x00,0x64,0x45};
 assert(len==9 && memcmp(b,example,9)==0);
 for(int i=0;i<512;i++){
  assert(codec::ch_manual(i%3,uint8_t(i%101),id,b,sizeof(b),len));
  assert(b[3]!=0x5a&&b[4]!=0x5a&&b[len-1]!=0x5a);
  assert(codec::ch_xor(b,len-1)==b[len-1]);
 }
 assert(codec::ch_auto(id,b,sizeof(b),len));assert(b[5]==0x05&&b[6]==18&&len==10);
 ProfileImage p{};p.pvCount=cielo::pezverde_count;p.chCount=cielo::chihiros_count;
 memcpy(p.pv,cielo::pezverde,sizeof(cielo::pezverde));memcpy(p.ch,cielo::chihiros,sizeof(cielo::chihiros));
 profileSeal(p);assert(profileValid(p));
 assert(cielo::at(p.pv,p.pvCount,1439).brightness==0);
 assert(cielo::at(p.ch,p.chCount,1380).r==0);
 p.pv[10].r^=1;assert(!profileValid(p));
 profileSeal(p);assert(profileValid(p));
 puts("OK: PV wire frames, CH RGB/AUTO with reserved bytes, double-slot snapshot seal, Natural v4 end-of-day");
}
