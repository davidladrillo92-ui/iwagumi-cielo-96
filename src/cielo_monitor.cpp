#include "cielo_monitor.h"
#include <stdio.h>
namespace cielo96 { namespace monitor {
namespace {const char* b(bool v){return v?"true":"false";}}
size_t encode(const Snapshot&s,char*out,size_t cap){
 // firmware es un literal de compilación, nunca entrada de red.
 int n=snprintf(out,cap,
 "{\"schema\":\"cielo-monitor-v1\",\"firmware\":\"%s\",\"boot_id\":%lu,\"uptime_sec\":%lu,"
 "\"epoch\":%lu,\"clock_valid\":%s,\"ble_ready\":%s,\"worker_stalled\":%s,\"ota\":%s,"
 "\"heap_free\":%lu,\"heap_min\":%lu,\"largest_block\":%lu,\"ble_stack_free_bytes\":%lu,"
 "\"queue_depth\":%lu,\"worker_queue_depth\":%lu,"
 "\"profile\":{\"generation\":%lu,\"pv_points\":%lu,\"ch_points\":%lu},"
 "\"pv\":{\"expected\":%s,\"ever_ok\":%s,\"ok_age_sec\":%lu,\"consecutive_errors\":%lu,\"operations\":%lu,\"errors\":%lu},"
 "\"ch\":{\"expected\":%s,\"ever_ok\":%s,\"ok_age_sec\":%lu,\"consecutive_errors\":%lu,\"operations\":%lu,\"errors\":%lu},"
 "\"ch_return_failed\":%s,\"off_pending\":%s,\"off_failures\":%lu,\"optical_feedback\":null}",
 s.firmware,(unsigned long)s.bootId,(unsigned long)s.uptime,(unsigned long)s.epoch,
 b(s.clockValid),b(s.bleReady),b(s.workerStalled),b(s.ota),
 (unsigned long)s.heap,(unsigned long)s.heapMin,(unsigned long)s.largestBlock,(unsigned long)s.stackFreeBytes,
 (unsigned long)s.queueDepth,(unsigned long)s.workerQueueDepth,
 (unsigned long)s.profileGeneration,(unsigned long)s.pvPoints,(unsigned long)s.chPoints,
 b(s.pv.expected),b(s.pv.everOk),(unsigned long)s.pv.okAgeSeconds,(unsigned long)s.pv.consecutiveErrors,(unsigned long)s.pv.operations,(unsigned long)s.pv.errors,
 b(s.ch.expected),b(s.ch.everOk),(unsigned long)s.ch.okAgeSeconds,(unsigned long)s.ch.consecutiveErrors,(unsigned long)s.ch.operations,(unsigned long)s.ch.errors,
 b(s.chReturnFailed),b(s.offPending),(unsigned long)s.offFailures);
 if(n<0||static_cast<size_t>(n)>=cap){if(cap)out[0]=0;return 0;}
 return static_cast<size_t>(n);
}
} }
