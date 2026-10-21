#include "cielo_diagnostic_policy.h"
#include <cassert>
#include <string>
using namespace cielo96::diag;
int main(){
 assert(std::string(name(Level::Ok))=="CORRECTO");
 assert(std::string(name(Level::Error))=="FALLO");
 assert(std::string(name(Level::Warning))=="ADVERTENCIA");
 assert(std::string(name(Level::NotTested))=="NO_PROBADO");
 assert(std::string(name(Level::NoData))=="SIN_DATOS");
 assert(link(false,false,false,false,false)==Level::NoData);
 assert(link(true,false,true,true,false)==Level::Error);
 assert(link(true,true,false,false,false)==Level::NotTested);
 assert(link(true,true,true,true,true)==Level::Warning);
 assert(link(true,true,true,true,false)==Level::Ok);
 assert(link(true,true,true,false,false)==Level::Warning);
 assert(fragmentationPct(1000,800)==20);
 assert(fragmentationPct(0,0)==-1);
 assert(fragmentationPct(1000,1500)==0);
 assert(!canReportP95(0) && !canReportP95(9) && canReportP95(10));
 assert(importantEvent("PV_POWER_OFF_REINTENTO"));
 assert(importantEvent("BLE_ERROR"));
 assert(importantEvent("REINICIO_ANOMALO"));
 assert(!importantEvent("WIFI_NUEVA"));
 assert(!importantEvent("HORA_SYNC"));
 assert(!shouldFlush(100000,50000,true));
 assert(shouldFlush(350000,50000,true));
 assert(shouldFlush(300,0,false)); // incidente en el primer minuto no se pierde
 assert(shouldFlush(100,0xfffaff00U,true)); // millis overflow
 return 0;
}
