# Incidente RC3 del 21/10/2026 y candidata RC5 completa

Nueva candidata: **11.1.0-RC5-B1-B9-RECOVERY**. Sustituye para pruebas a la
candidata MONITOR de la PR #34; conserva los nueve bloques y V5. No instalada.

## Evidencias y conclusión

Informes privados 41 (21:48:40, uptime 25.375 s) y 42 (23:46:03, uptime 7.017 s).
No se publican los JSON completos ni sus identificadores de dispositivos.

| Indicador | Informe 41 | Informe 42 |
|---|---:|---:|
| Firmware instalado | RC3 | RC3 |
| Tarea BLE | CONEXIÓN, dispositivo 0/Pez Verde | ESPERA |
| Edad de fase | 1.358.846 ms | 113 ms |
| Edad de heartbeat | 1.359.609 ms | 113 ms |
| Worker sin respuesta | Sí | No |
| Wi-Fi caído | 0 veces | 0 veces |
| RAM interna libre / bloque mayor | 130.096 / 118.772 B | 133.760 / 118.772 B |
| RAM mínima | 81.524 B | 82.956 B |
| Curva / generación | PV34 CH31 / 8 | PV34 CH31 / 8 |
| Operaciones GATT PV / CH finalizadas OK | 131 / 123 | 114 / 74 |
| Reinicio | Software, previo a la sesión | POWER_ON, corte manual |

El bloqueo empezó aproximadamente a las **21:26:01**, tras unas 6 h 40 min de
sesión. A los 45 s RC3 desactivó el control y pidió retorno Chihiros, pero la misma
tarea BLE seguía bloqueada: no podía ejecutar ni retorno ni nuevas conexiones.
Al generar el informe llevaba 22 min 39 s en esa fase. El reinicio manual siguiente
se sitúa aproximadamente a las 21:49:06. Los tiempos se reconstruyen del reloj y
uptime de los informes, no de una captura independiente de radio.

**Causa inmediata acreditada:** la única tarea BLE no progresaba dentro de la
conexión a Pez Verde, bloqueando también las operaciones de Chihiros. No son dos
fallos independientes demostrados. **Desencadenante interno no demostrado:** los
informes no contienen la subfase de registro/apertura de GATT, código de evento o
backtrace que permita decidir entre callback no recibido, carrera, controlador,
interferencia o pantalla. No se atribuye sin pruebas al móvil, alimentación o NVS.

Arduino-ESP32 2.0.17 usa esperas de semáforo sin límite (`portMAX_DELAY`) dentro
de `BLEClient::connect()`. Esto explica cómo puede quedar bloqueada una llamada,
pero no identifica cuál de sus esperas falló en esta placa.
Fuentes: [BLEClient.cpp](https://github.com/espressif/arduino-esp32/blob/2.0.17/libraries/BLE/src/BLEClient.cpp)
y [FreeRTOS.cpp](https://github.com/espressif/arduino-esp32/blob/2.0.17/libraries/BLE/src/FreeRTOS.cpp).

El campo `connected:false` por sí solo NO era el fallo: también aparece en el
informe 42 sano porque CIELO libera normalmente los enlaces tras 5 s inactivos.
`queued:255` en RC3 cuenta órdenes acumuladas, no 255 órdenes atascadas.
`max_call_ms` mide llamadas que terminaron, excluye la conexión colgada.
Los checks de laboratorio del informe 41 proceden de una única muestra antigua;
no contradicen el bloqueo actual. RC5 distingue contadores, antigüedad y estado.

El único retorno 0x03 OK del informe 41 ocurrió al principio de la sesión, NO después
del bloqueo. No acredita que My Chihiros recuperase su curva aquella noche.
En el informe 42 hay otro retorno GATT tras reinicio y comunicaciones posteriores
correctas; la observación visual del propietario complementa esa evidencia.

El timeout blando de 3,5 s durante arranque también aparece después del reinicio:
la operación acabó en 6,716 s. Es una llamada lenta recuperada, no otro bloqueo duro.
La caída de tensión, agotamiento de RAM o flash llena no quedan demostrados.

## Lo que faltaba y lo que cambia

La candidata MONITOR protegía apagado/reintentos cuando el worker volvía y añadía
alertas externas, pero no podía desbloquear una llamada que nunca regresaba.
Por tanto **la situación podía repetirse** sin intervención automática eficaz.

Ahora hay un supervisor local que:

1. Exige simultáneamente heartbeat y fase sin progreso durante 120 s.
2. Registra fase, dispositivo, duración e intentos en RTC con comprobación de integridad.
3. Reinicia el MCU completo: no mata una tarea con callbacks vivos ni manipula BLE
   desde el hilo web. No borra flash, NVS, certificados, Wi-Fi o curvas.
4. Permite como máximo dos reinicios automáticos; solo repone el presupuesto tras
   30 min continuos con worker ágil. Al agotarlo conserva web y señala intervención.
5. No reinicia durante preparación/carga OTA ni modo de rescate.
6. Tras su reinicio, si Pez Verde estaba bajo control automático, encola POWER OFF
   antes del retorno Chihiros. Retiene la reanudación hasta completar ese GATT,
   superar el arranque de 180 s y tener hora y perfil válidos. Luego usa la consigna
   actual, sin reproducir RGB viejos. Puede enviar OFF aunque aún no haya hora.

El supervisor depende de que siga ejecutándose el bucle principal, como sucedía
en el informe 41. No se presenta como watchdog físico ni como garantía de apagar
una lámpara sin enlace BLE. Durante reinicio no hay control de luces y el apagado
sigue sin confirmación óptica. Un fallo persistente puede agotar el límite; sigue
siendo necesaria una protección eléctrica independiente para la ausencia.

## Memoria y limpieza

Los dos informes ya confirman 34/31, generación 8 y sin recuperación de defaults:
actualizan la evidencia anterior del informe 40, que tenía 34/32. Los conteos no
prueban por sí solos todos los RGB; exportar el perfil real antes de instalar.

No hay prueba de «basura» que cause este fallo. La flash tiene particiones con
funciones distintas: aplicaciones OTA, configuración NVS y posible filesystem.
Restos de datos invalidados en NVS forman parte de su gestión normal. No ejecutar
borrado total ni formateo por intuición: quitaría claves y configuración sin resolver
una espera BLE sin límite. El backup permite inspeccionar límites, contenido vacío
y bitmaps NVS sin mostrar valores privados. Un bitmap no valida CRC de entradas,
claves ni filesystem; estados desconocidos requieren investigación, no borrado.

## Verificación y límites

Pruebas host: reproducción de los valores 41/42, límites temporales, exclusión OTA,
presupuesto entre reinicios, corrupción RTC, rearme y vuelta de millis; contrato del
interbloqueo OFF, suites previas V5/B1–B9 y copia cifrada. Compilación ESP32-S3 y
Actions deben corresponder al commit final publicado en PR #34.
No se ha provocado un bloqueo en el acuario, ejecutado USB ni probado Safari físico.

Antes de despliegue sin supervisión se necesita ensayo de bloqueo real en banco
con las lámparas protegidas, POWER OFF óptico, retorno 0x03, OTA/rollback y 24–72 h
de funcionamiento. Una compilación correcta no sustituye esos ensayos.
