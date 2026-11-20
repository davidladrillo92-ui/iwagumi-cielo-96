#include "cielo_recovery313.h"
#include "cielo_diagnostic_evidence.h"
#include <assert.h>
int main(){
 using namespace cielo96::r313;
 assert(diagnosticReserve(true,832)==152);
 assert(diagnosticReserve(true,832)<=156);
 assert(diagnosticReserve(false,832)==cielo96::nvssafe::diagnosticReserve(832));
 assert(olderThanKnown(9,22)&&!olderThanKnown(22,22)&&!olderThanKnown(23,22));
 assert(!olderThanKnown(1,0)&&olderThanKnown(0,22));
 assert(!olderThanKnown(1,0xffffffffu));
 assert(elapsedMs(1000,4294967297000ULL)==4294967296ULL);
 assert(elapsedMs(1000,4294967397000ULL)==4294967396ULL);
 assert(!lampFault(false,false,false,false));
 assert(!lampFault(true,true,true,false)); // planned OFF counts as fresh
 assert(lampFault(true,false,true,false));
 assert(lampFault(true,true,false,false));
 assert(!lampFault(true,false,false,true));
 cielo96::evidence::OperationalHistory history;
 history.capture("PV_DIA_REANUDADO",4294967396ULL,0);
 assert(history.at(0).uptime_ms==4294967396ULL);
}
