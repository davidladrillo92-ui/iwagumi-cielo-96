#include "cielo_ble_presence.h"
#include <cassert>
using namespace cielo96::presence;
int main(){
 assert(!recent(true,0,1000,0));
 assert(recent(true,1000,91000,0));
 assert(!recent(true,1000,91001,0));
 assert(!recent(false,1000,1001,0));
 assert(!recent(true,1000,1001,1));
 // Report 62: snapshot still connected, but last success is 247 seconds old.
 assert(!recent(true,1000,248000,0));
 assert(recent(true,0xfffffff0U,0x20U,0));
 assert(!refreshDue(59999,0));assert(refreshDue(60000,0));
 assert(refreshDue(59984,0xfffffff0U));
}
