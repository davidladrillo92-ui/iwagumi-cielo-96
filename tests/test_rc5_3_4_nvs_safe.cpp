#include <cassert>
#include <cstring>
#include "cielo_nvs_safe.h"
using namespace cielo96::nvssafe;
int main(){
 assert(minimumFreeEntries(0)==2);
 assert(minimumFreeEntries(1)==3);
 assert(minimumFreeEntries(32)==3);
 assert(minimumFreeEntries(33)==4);
 assert(definitelyTooLittleSpace(3,33));
 assert(!definitelyTooLittleSpace(4,33));
 assert(std::strcmp(target(false,0,false,0),"curvesA")==0);
 assert(std::strcmp(target(true,8,false,0),"curvesB")==0);
 assert(std::strcmp(target(false,0,true,8),"curvesA")==0);
 assert(std::strcmp(target(true,8,true,7),"curvesB")==0);
 assert(std::strcmp(target(true,7,true,8),"curvesA")==0);
 assert(targetPreservesOnlyCopy(true,false,target(true,8,false,0)));
 assert(targetPreservesOnlyCopy(false,true,target(false,0,true,8)));
 assert(!targetPreservesOnlyCopy(true,false,"curvesA"));
 assert(!targetPreservesOnlyCopy(false,true,"curvesB"));
 assert(std::strstr(message(Failure::InsufficientEntries),"insuficiente")!=nullptr);
 assert(std::strstr(message(Failure::WriteFailed),"escritura")!=nullptr);
 assert(std::strstr(message(Failure::VerificationFailed),"verificacion")!=nullptr);
}
