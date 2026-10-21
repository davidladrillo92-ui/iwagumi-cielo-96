#include "cielo_night_safety.h"
#include <cassert>
#include <cstdint>
using namespace cielo96::safety;
int main(){
 // Antes de medianoche: curva aun iluminada; al entrar en cero hay obligacion.
 assert(!offRequired(true,true,true,24));
 assert(offRequired(true,true,true,0));
 // No inventar una hora valida cuando el reloj se pierde tras un corte.
 assert(!offRequired(true,false,true,0));
 // Tras recuperar reloj durante la noche, volver a exigir POWER OFF.
 assert(offRequired(true,true,true,0));
 // Ninguna orden de dia guardada previamente puede pasar la barrera nocturna.
 assert(forbidStaleRgb(true,24));
 assert(!forbidStaleRgb(true,0));
 // Solo el resultado de la orden POWER OFF en vuelo confirma GATT.
 assert(!isOffAck(true,72,71));
 assert(isOffAck(true,72,72));
 assert(!isOffAck(false,72,72));
 // Tras 3 fallos no cesa la obligacion; solo crece la espera entre intentos.
 assert(offRetryDelayMs(1)==5000UL);
 assert(offRetryDelayMs(3)==15000UL);
 assert(offRetryDelayMs(5)==60000UL);
 // millis() ciclico y reconexion tardia (00:05) conservan orden de reintento.
 assert(!retryDue(1000,4000));
 assert(retryDue(5000,4000));
 assert(retryDue(100,0xfffffff0UL));
 return 0;
}
