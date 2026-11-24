#include "cielo_health314.h"
#include <cassert>
#include <cstdio>
using namespace cielo96::health314;
int main(){
 OffSummary off;Incidents incidents;incidents.capture("BLE_ERROR",100);
 for(unsigned i=0;i<144;i++){assert(off.capture("PV_POWER_OFF_ENCOLADO",i*300000ULL+1));assert(off.capture("PV_POWER_OFF_INTENTO",i*300000ULL+2));assert(off.capture("PV_POWER_OFF_GATT_OK",i*300000ULL+3));}
 assert(off.count[2]==144&&off.first[2]==3&&off.last[2]==143*300000ULL+3);
 assert(!off.capture("PV_POWER_OFF_REINTENTO",999));
 assert(incidents.count==1);incidents.capture("BLE_ERROR",200);assert(incidents.at(0).repeats==2);
 for(unsigned i=0;i<20;i++){char code[20];snprintf(code,sizeof(code),"ERR_%u",i);incidents.capture(code,300+i);}
 assert(incidents.count==16&&!strcmp(incidents.at(15).code,"ERR_19"));
 assert(!optionalWriteAllowed(true,155,152,16));assert(optionalWriteAllowed(true,168,152,16));
 assert(!optionalWriteAllowed(false,1000,152,16));assert(!optionalWriteAllowed(true,151,152,0));
 assert(!strcmp(pressureName(true,155,152),"LOW_MARGIN"));assert(!strcmp(pressureName(false,0,152),"UNKNOWN"));
 assert(diagnosticRetryMs(true)==1800000&&diagnosticRetryMs(false)==300000);
 Trend t;Sample good{};good.heap=102728;good.fragmentation=13;good.pv95=33;good.ch95=305;good.busyBp=550;good.stack=9932;good.nvsFree=155;good.nvsReserve=152;good.nvsValid=good.latencyValid=good.queueValid=true;
 for(int i=0;i<5;i++)t.sample(good);
 assert(t.flags==0);
 Sample bad=good;bad.heap=70000;bad.fragmentation=50;bad.pv95=400;bad.ch95=1200;bad.busyBp=3000;bad.stack=2500;bad.nvsFree=145;
 t.sample(bad);t.sample(bad);assert(!t.flags);t.sample(good);assert(!t.flags);
 for(int i=0;i<3;i++)t.sample(bad);
 assert(t.flags==127);
 for(int i=0;i<3;i++)t.sample(good);
 assert(!t.flags);
 bad.nvsValid=false;bad.latencyValid=false;bad.queueValid=false;
 for(int i=0;i<3;i++)t.sample(bad);
 assert(t.flags==(Heap|Fragmentation|Stack));
 BleClass ble;ble.observe(1000,true,5785,5000,121);assert(ble.startupSlow==1&&ble.recoveredSlow==1&&!ble.recentWarning(1000));
 ble.observe(5000,true,4000,1000,20);assert(ble.reconnectSlow==1);
 ble.observe(6000,true,5000,0,0);ble.observe(10000,false,6000,0,0);assert(ble.recentWarning(10001)&&ble.stableSlow==2&&ble.failedSlow==1);
 assert(!ble.recentWarning(310001));ble.observe(20000,true,33,0,0);assert(!ble.recentWarning(20000));
 ble.softResult(true,true);ble.softResult(true,false);ble.softResult(false,true);assert(ble.matchedSoftTimeoutOk==1&&ble.matchedSoftTimeoutFailed==1);
 Levels a{5,7,12,0},b{0,0,0,0},c{100,50,20,50};
 assert(equivalent(a,b,true)&&!equivalent(a,b,false));
 assert(!strcmp(phase(a,b,c,true),"OFF"));assert(!strcmp(phase(b,c,Levels{100,50,20,60},true),"RISING"));
 assert(!strcmp(phase(c,c,c,true),"STEADY"));
 Delivery d;d.dispatch(7,c,10000);assert(!d.finish(6,true,true,10001,false));assert(d.pending&&!d.hasAck);
 assert(!d.finish(7,false,true,10010,false));assert(d.pending);
 assert(d.finish(7,true,true,10040,false)&&equal(d.ack,c)&&d.lastMs==40);
 d.invalidate();assert(!d.ackApplicable);
 d.dispatch(8,c,20000);assert(d.finish(8,true,true,20070,true)&&equal(d.ack,b)&&d.maxMs==70);
 d.dispatch(9,c,30000);assert(!d.finish(9,true,false,30010,false)&&!d.ackApplicable);
 assert(!strcmp(summary(false,false,false),"OPERATIVO"));assert(!strcmp(summary(false,false,true),"OPERATIVO_CON_AVISOS"));assert(!strcmp(summary(true,true,true),"INCIDENTE_ACTUAL"));
 puts("PASS 3.14 V2: 144 OFF cycles, independent incidents, NVS margin, degradation debounce, BLE result identity and profile evidence");
}
