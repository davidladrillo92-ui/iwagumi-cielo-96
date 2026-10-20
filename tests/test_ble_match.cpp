#include "cielo_ble_match.h"
#include <cassert>
#include <iostream>

int main(){
 using cielo96::bleNameMatches;
 assert(bleNameMatches("ELK-BLEDOB","ELK-BLEDOB"));
 assert(!bleNameMatches("ELK-BLEDOB2","ELK-BLEDOB"));
 assert(!bleNameMatches("ELK","ELK-BLEDOB"));
 assert(bleNameMatches("DYSL60","DYSL60"));
 assert(bleNameMatches("DYSL60D1B0E18A7253","DYSL60"));
 assert(bleNameMatches("DYSL60-OTHER","DYSL60"));
 assert(!bleNameMatches("DYSL61D1B0E18A7253","DYSL60"));
 assert(!bleNameMatches("WRGB-DYSL60","DYSL60"));
 assert(!bleNameMatches("","DYSL60"));
 assert(!bleNameMatches(nullptr,"DYSL60"));
 assert(!bleNameMatches("DYSL60",nullptr));
 std::cout<<"PASS BLE matching: Pez Verde exact, Chihiros prefijo comprobado"<<std::endl;
}
