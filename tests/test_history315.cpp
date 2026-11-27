#include "cielo_history315.h"
#include <cassert>
#include <cstdlib>
#include <cstdio>
using namespace cielo96::history315;
struct Mock { Image im[2]{};Read state[2]={Read::Missing,Read::Missing};bool torn=false;unsigned writes=0;
 Read read(unsigned i,Image& x){if(state[i]==Read::Valid)x=im[i];return state[i];}
 bool write(unsigned i,const Image& x){++writes;im[i]=x;state[i]=torn?Read::Invalid:Read::Valid;return !torn;}
};
int main(){
 Mock m;Ring r;Store<Mock> a(m);assert(a.load(r));r.sample(2026330,10,20,0,0,100000);a.dirty=true;
 const uint32_t ep=1795700000;assert(!a.checkpoint(r,bootQuietMs-1,ep,true));
 assert(!a.checkpoint(r,bootQuietMs,ep,false));assert(!a.checkpoint(r,bootQuietMs+1,ep,true));
 assert(a.checkpoint(r,bootQuietMs+retryMs,ep,true));assert(a.generation==1&&m.writes==1);
 r.sample(2026330,1,2,0,0,90000);a.dirty=true;
 assert(!a.checkpoint(r,bootQuietMs+retryMs+1,ep+999999,true)); // wall clock jump cannot accelerate wear
 m.torn=true;assert(!a.checkpoint(r,2ULL*periodMs+retryMs,ep+21600,true));assert(a.active==0&&a.dirty);
 Ring restored;Store<Mock> b(m);assert(b.load(restored));assert(b.generation==1&&restored.oldest(0).pv_ok==10);
 m.torn=false;restored.sample(2026330,3,4,0,0,80000);b.dirty=true;assert(b.checkpoint(restored,bootQuietMs,ep+21600,true));
 assert(b.active==1&&b.generation==2);Ring again;Store<Mock> c(m);assert(c.load(again));assert(again.oldest(0).pv_ok==13&&again.oldest(0).samples==2);
 assert(!c.checkpoint(again,periodMs,ep+21601,true));
 m.im[1].crc^=1;m.state[1]=Read::Invalid;Ring fallback;Store<Mock> d(m);assert(d.load(fallback));assert(d.generation==1);
 m.state[1]=Read::Foreign;Ring conflict;Store<Mock> ro(m);assert(ro.load(conflict)&&ro.blocked&&conflict.count()==1);
 m.state[0]=Read::Invalid;m.state[1]=Read::Invalid;Ring both;Store<Mock> e(m);assert(!e.load(both)&&e.blocked);
 assert(validDay(2026364)&&!validDay(2026365)&&validDay(2028365));
 char root[]="/tmp/cielo-hist-XXXXXX";assert(mkdtemp(root));Posix io(root);Ring q;Store<Posix> p(io);assert(p.load(q));q.sample(2026330,8,9,0,0,80000);p.dirty=true;assert(p.checkpoint(q,bootQuietMs,ep,true));
 Ring q2;Store<Posix> p2(io);assert(p2.load(q2)&&q2.oldest(0).ch_ok==9);char path[128];snprintf(path,sizeof(path),"%s/cielo_hist315_a.bin",root);assert(!unlink(path));assert(!rmdir(root));
 puts("PASS history A/B, real POSIX, torn write, CRC fallback, replay, low space, monotonic wear, leap year");
}
