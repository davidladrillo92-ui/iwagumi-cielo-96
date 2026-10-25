#include "cielo_monitor.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(){
 cielo96::monitor::Snapshot m{};m.firmware="11.1.0-RC5-B1-B9-MONITOR";
 m.bootId=UINT32_MAX;m.uptime=UINT32_MAX;m.epoch=UINT32_MAX;
 m.heap=m.heapMin=m.largestBlock=m.stackFreeBytes=UINT32_MAX;
 m.queueDepth=m.workerQueueDepth=m.profileGeneration=UINT32_MAX;m.pvPoints=34;m.chPoints=31;
 m.pv={true,true,UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX};m.ch=m.pv;
 m.offFailures=UINT32_MAX;m.offPending=true;
 m.recentCount=8;m.restartTotal=UINT32_MAX;m.lastStallMs=UINT32_MAX;
 for(auto&e:m.recent){e.boot=e.previous=e.epoch=e.stallEpoch=e.stallMs=e.recoveredEpoch=UINT32_MAX;
  e.reset=9;e.device=255;e.phase=11;e.bleRecovery=true;e.outcome=1;}
 char out[cielo96::monitor::JSON_CAPACITY];
 size_t n=cielo96::monitor::encode(m,out,sizeof(out));assert(n>0&&n<sizeof(out));
 assert(strstr(out,"\"optical_feedback\":null"));assert(strstr(out,"\"pv_points\":34"));
 assert(!strstr(out,"password"));
 char tiny[8]={'x'};assert(cielo96::monitor::encode(m,tiny,sizeof(tiny))==0&&tiny[0]==0);
 assert(cielo96::monitor::encode(m,nullptr,0)==0);
 puts(out);
}
