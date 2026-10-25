# CIELO — Estudio integral de la aplicación y consolidación RECOVERY3.2-APP

Fecha del estudio: 26/10/2026. Base de código: RECOVERY3.1 integrada; comparación directa con RECOVERY3 y SIMPLE1. No es una reescritura de CIELO.

## Fuentes y hechos observados
- Informe 69 (25/10, 22:31): RECOVERY3, arranque 123, 95/95 PV y 92/92 CH GATT OK, Wi-Fi sin caídas, fallo BLE previo recuperado.
- Informe 70 (25/10, 23:06): RECOVERY3.1, arranque 124, 6/6 PV y 2/2 CH GATT OK, OTA postarranque correcto, GATT no mide luz física.
- Informe 70 contiene boot_epoch y primera muestra nocturna/diaria = 1867341431 (marzo 2029), pese a generated_epoch=1792965996 (octubre 2026). Evento HORA_SYNC por teléfono a 194487 ms, posterior al registro inicial.
- Captura 26/10 00:20: Pez Verde apagada físicamente según usuario; UI mostraba 6 %, RGB 58/78/132; Chihiros 0 %. «Conectado» visible en ambos equipos.

## Hallazgos y causa
1. **6 % nocturno**: `render()` priorizaba `pv_last_requested` si GATT era reciente, incluso tras una orden POWER OFF independiente. La última consigna RGB no representa el estado de alimentación. Este patrón ya existía en SIMPLE1 y RECOVERY3; **no hay prueba de que lo introdujera RECOVERY3.1**. Su detección actual no equivale a regresión nueva.
2. **2029**: `timeOK()` solo exigía epoch posterior a noviembre de 2023. En arranque 124 se aceptó una hora futura como válida antes de la sincronización telefónica, y se grabó en `restartJournal` y `cieloTrend`. Se ha demostrado la secuencia, pero **no el origen físico de la hora incorrecta** (RTC heredado, NTP u otra inicialización). No atribuirlo al móvil sin pruebas: la sincronización de móvil llegó después y corrigió la hora del informe.
3. **23:00–24:00**: la escena estaba etiquetada «Descanso nocturno» aun cuando Pez Verde conservaba luz tenue. La escena ahora dice «Luz de luna» cuando la curva realmente tiene brillo mayor que cero.
4. **Conectado**: mantiene semántica de comunicación GATT reciente con enlace BLE liberado entre envíos; no prueba luz real ni enlace permanente.
5. **Centro**: no se reescriben sus diagnósticos, controles ni OTA. Se conserva comportamiento probado.
6. **Seguridad pendiente**: LAN HTTP y OTA sin contraseña ni firma obligatoria según informe 70. No modificar aquí para no mezclar cambios de seguridad con la estabilización de la app; es una limitación documentada, no una funcionalidad segura.

## Correcciones de alcance mínimo
- UI (fuente y versión realmente embebida): cuando `pv_off_obligation=true` y `pv_safety_state=APAGADO_GATT_OK`, presentar 0 % y RGB 0/0/0. Mantener «Conectado» como comunicación; tooltip «GATT aceptado, sin sensor óptico». Si OFF está pendiente, mostrar — y «Apagado pendiente», sin afirmar que está apagada. En vista previa de curvas, mostrar simulación sin mezclar estado real.
- Hora: registro de fecha en informes y monitorización solo cuando hay **sincronización nueva de NTP o del teléfono en el arranque actual**. Antes, presentar hora pendiente, no publicar un epoch futuro como válido. Reparar el boot_epoch actual al recibir una hora fiable. Conservar horas históricas reales. La hora del móvil se usa solo como referencia de presentación mientras el ATOM espera sincronización.
- Se preserva `timeOK()` en el **motor de curvas**, por prudencia: no se cambia el algoritmo de encendido/apagado ni el modo de autonomía. Un reloj erróneo antes de NTP sigue siendo un riesgo operativo independiente; abordarlo requiere un diseño de reloj persistente y pruebas de arranque sin internet. El cambio presente evita falsos informes y señales visuales de confianza.
- Sin cambios en curvas V5, BLE, prioridades, watchdog, reinicios, Wi-Fi, Telegram Sierra, NVS de perfiles ni OTA.

## Matriz de regresión
- Apagado nocturno con última consigna 6 %: interfaz 0 % solo tras OFF GATT OK.
- OFF pendiente o fallido: no mostrar 0 % como apagado confirmado.
- Día con última consigna 6 % y sin OFF: mostrar 6 %.
- 23:30 con brillo >0: escena Luz de luna; 00:20: Descanso nocturno.
- Reloj sin sincronizar: hora pendiente en estado e informes; no crear muestras con 2029.
- Hora sincronizada por NTP o móvil: fecha coherente y boot_epoch reparado.
- Curvas, perfiles y pantallas siguen intactos. Tests automáticos son **host**, no sustituyen validación física.

## Criterios de cierre
1. CI de Python, Node, C++ y PlatformIO completa; fuentes HTML y embebido idénticos.
2. Confirmar en iPhone e iPad: Luz, Curvas, Centro, actualización, errores y retorno tras Wi-Fi.
3. En noche real: PV apagada, 0 % mostrado, «Conectado» cuando GATT reciente; no confirmar ópticamente sin observación.
4. Arranque con RTC artificialmente incorrecto, con y sin NTP, y posterior sincronización telefónica; revisar Informe Maestro y Telegram.
5. Mantener versión instalada anterior disponible para rollback. No fusionar ni considerar definitiva hasta cumplir pruebas físicas y un ciclo completo de 24–72 h.

**Estado:** candidata conservadora, pendiente de pruebas físicas. La app no está declarada «cerrada» por una compilación exitosa.
