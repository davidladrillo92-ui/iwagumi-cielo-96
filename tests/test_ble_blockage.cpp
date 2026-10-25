#include "cielo_ble_blockage.h"
#include <cassert>
#include <cstring>
using namespace cielo96::blockage;
int main(){
 const auto healthy=classify({2,255,100,100,0,0});
 assert(healthy.primary==Kind::None);
 const auto sdk=classify({2,255,200,200,0,120000});
 assert(sdk.primary==Kind::SdkConnectPending);
 assert(sdk.pending_device==1 && sdk.pending_age_ms==120000);
 assert(!sdk.worker_unresponsive && !sdk.discovery_wait);
 const auto svc=classify({5,1,120000,120000,0,0});
 assert(svc.primary==Kind::GattDiscoveryWait);
 assert(svc.discovery_wait && !svc.sdk_pending);
 const auto chr=classify({6,0,20000,20000,0,0});
 assert(chr.primary==Kind::GattDiscoveryWait);
 const auto worker=classify({7,0,120000,120000,0,0});
 assert(worker.primary==Kind::WorkerUnresponsive);
 const auto both=classify({5,1,120000,120000,0,150000});
 assert(both.primary==Kind::GattDiscoveryWait && both.sdk_pending);
 assert(both.pending_age_ms==150000);
 const auto pv=classify({2,255,100,100,9000,0});
 assert(pv.primary==Kind::SdkConnectPending && pv.pending_device==0);
 const auto tie=classify({2,255,100,100,9000,9000});
 assert(tie.pending_device==0); // stable tie-breaker
 assert(std::strcmp(name(Kind::None),"NONE")==0);
 assert(std::strcmp(name(Kind::GattDiscoveryWait),"GATT_DISCOVERY_WAIT")==0);
 return 0;
}
