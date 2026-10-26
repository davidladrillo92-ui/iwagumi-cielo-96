#include "cielo_littlefs_store.h"
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace cielo96;
int main(){
 char root[]="/tmp/cielo-lfs-vfs-XXXXXX";assert(mkdtemp(root));
 littlecurve::PosixIO io(root);io.mounted(true);lfs_mounted=true;
 littlecurve::Publisher<littlecurve::PosixIO> store(io);
 ProfileImage p{};p.generation=9;p.pvCount=p.chCount=2;
 p.pv[1].minute=p.ch[1].minute=1440;profileSeal(p);
 // A user/partial stage is never truncated, opened or removed by publishing.
 const auto foreign=std::filesystem::path(root)/"cielo_lfs_stage_00.bin";
 {std::ofstream f(foreign);f<<"preserve this";}
 assert(store.save(p)&&store.preservedStages==1&&io.writeBytes==788&&io.createCalls==1&&io.syncCalls==1);
 ProfileImage read{};assert(store.newest(read)&&memcmp(&p,&read,sizeof(p))==0);
 uint32_t writes=io.writeBytes,creates=io.createCalls,renames=io.renameCalls;
 for(unsigned i=0;i<1000;i++)assert(store.save(p)&&store.unchanged);
 assert(io.writeBytes==writes&&io.createCalls==creates&&io.renameCalls==renames);
 assert(!io.createExclusive(littlecurve::kA,p)&&io.lastErrno==EEXIST);
 auto q=p;q.generation=10;q.pv[0].r=3;profileSeal(q);
 assert(store.save(q)&&store.newest(read)&&memcmp(&q,&read,sizeof(q))==0);
 auto r=q;r.generation=11;r.pv[0].r=4;profileSeal(r);
 assert(store.save(r)); // actual POSIX rename replaces an existing inactive slot
 assert(io.read(littlecurve::kB,read)==littlecurve::Read::Valid&&memcmp(&q,&read,sizeof(q))==0);
 assert(store.newest(read)&&memcmp(&r,&read,sizeof(r))==0);
 {std::ifstream f(foreign);std::string s;std::getline(f,s);assert(s=="preserve this");}
 lfs_used=lfs_total-32767;auto low=r;low.generation++;profileSeal(low);
 writes=io.writeBytes;assert(!store.save(low)&&io.writeBytes==writes&&io.lastErrno==ENOSPC);
 lfs_used=72*4096;lfs_info_error=ESP_FAIL;
 assert(!store.save(low)&&io.lastErrno==EIO);lfs_info_error=ESP_OK;
 std::filesystem::remove_all(root);
 // Production mount guard: no writable registration without successful RO.
 lfs_mounted=false;lfs_register_calls=0;lfs_allow_rw=true;
 storage::LittlefsReadonlyProbe probe;littlecurve::Store backend;
 assert(!backend.begin(probe)&&lfs_register_calls==0);
 probe.verified=true;lfs_mounted=true;
 assert(!backend.begin(probe)&&lfs_register_calls==0);lfs_mounted=false;
 assert(backend.begin(probe)&&backend.ready()&&lfs_register_calls==1);
 assert(!esp_vfs_littlefs_unregister("spiffs"));
 std::cout<<"PASS actual POSIX exclusive create/write/fsync/close/read/atomic replacement; no-op, foreign stages, headroom, info error and RO mount gate\n";
}
