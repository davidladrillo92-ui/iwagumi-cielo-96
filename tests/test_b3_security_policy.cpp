#include "cielo_security_policy.h"
#include <cassert>
using namespace cielo96::security;
int main(){
 assert(sessionFresh(true,1000,0,0));
 assert(sessionFresh(true,900000,0,0));
 assert(!sessionFresh(true,900001,0,0));
 assert(!sessionFresh(true,7200001UL,0,7199900UL));
 assert(!sessionFresh(false,10,0,0));
 assert(retryBefore(3000,3200));assert(!retryBefore(3200,3200));
 assert(retryBefore(0xfffffff0UL,100)); // millis desborda
 assert(sameOrigin("https://iwagumi-cielo96.local","iwagumi-cielo96.local"));
 assert(sameOrigin("https://192.168.0.25","192.168.0.25"));
 assert(!sameOrigin("https://fake.com","iwagumi-cielo96.local"));
 assert(!sameOrigin("http://iwagumi-cielo96.local","iwagumi-cielo96.local"));
 assert(!sameOrigin("https://iwagumi-cielo96.local.attacker","iwagumi-cielo96.local"));
 assert(!sameOrigin("https://iwagumi-cielo96.local","iwagumi-cielo96.local\r\nX:1"));
 assert(sameOrigin("","iwagumi-cielo96.local")); // necesita token en backend
 assert(safeFetchSite("same-origin"));assert(safeFetchSite("none"));
 assert(!safeFetchSite("cross-site"));assert(!safeFetchSite("same-site"));
 return 0;
}
