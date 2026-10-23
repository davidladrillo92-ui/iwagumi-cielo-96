# RC6 RECOVERY1 · candidata tras dos bloqueos de conexión

Fuente de evidencia: [incidencia 35](https://github.com/davidladrillo92-ui/iwagumi-cielo-96/issues/35), informes privados 49–58 resumidos allí. Base: SIMPLE1 `fa2eb676d23f711d5d49e7a37c1eed020e7d4326`. No contiene informes privados, credenciales ni volcados de flash.

## Problema observado

| Arranque | Fecha aproximada en Madrid | Destino | Fase retenida | Duración | Resultado posterior |
|---|---|---|---|---|---|
| 110 | 23/10 00:37 | Chihiros | CONEXION | 120.000 ms | Reinicio supervisor; comunicaciones recuperadas |
| 111 | 23/10 11:16:44 | Pez Verde | CONEXION | 120.001 ms | Reinicio supervisor; comunicaciones recuperadas |

La causa física o de radio no está demostrada. El código Bluedroid de Arduino-ESP32 2.0.17 usa esperas de semáforo sin plazo en `BLEClient::connect`. Es una vulnerabilidad al bloqueo, no una prueba de qué evento faltó en estos dos casos. RSSI Wi-Fi no mide la calidad BLE.

## Implementación

### P0: transporte y apagado

- Cambia solo el transporte a **NimBLE-Arduino 2.3.6 fijado**. Inicia la conexión de forma asíncrona, espera cooperativamente un máximo de **8.000 ms** y utiliza `cancelConnect` al vencer. No elimina tareas con callbacks pendientes. Desactiva el intercambio de MTU porque las tramas verificadas caben en el MTU ATT por defecto.
- Descubrimiento activo limitado a 5 s y 32 dispositivos. Dos clientes permanentes, sin autodestrucción desde callbacks; fallos de conexión invalidan caché de dirección y provocan descubrimiento posterior.
- Conserva los enlaces mientras se requiere control automático; cesión a My Chihiros y preparación OTA siguen liberándolos. Callbacks de desconexión actualizan el estado visible. Una desconexión pendiente no permite iniciar otra conexión sobre el mismo cliente.
- El envío GATT devuelve ahora el resultado real de la API de escritura. Sigue sin representar observación óptica.
- Snapshot coherente de fases y heartbeat bajo una sección crítica breve. Se evita restar un timestamp de otra tarea leído después de obtener `now` y confundir un rollover con un bloqueo.
- El reinicio de **120 s / máximo 2 / rearmado tras 30 min saludables** se conserva como último recurso ante una avería del stack. No se amplía ni se considera solución al bloqueo ordinario. Las operaciones GATT tienen el timeout ATT de la biblioteca (30 s); un atasco del host que impida sus callbacks todavía puede necesitar el supervisor. No se promete que toda clase de avería BLE quede eliminada.
- POWER OFF de PV precede a RGB cuando está listo, respetando cooldown para que la otra pantalla pueda avanzar. Cada encendido con control PV solicitado aplica primero OFF y después recupera la consigna horaria. Una bandera atómica se vuelve a comprobar después de conectar y entre tramas para impedir brillo obsoleto cuando llega el apagado.

### P0/P1: reinicios y Telegram

- Observador **4.0.0**, compatible con el endpoint de SIMPLE1. El primer cambio de `boot_id` genera un aviso inmediato, con independencia de tres reinicios/hora. Detecta además retroceso de uptime sin cambio de ID, etiquetado como contador inconsistente; excluye el rollover de `millis`.
- SQLite conserva la línea de base y la bandeja. Reiniciar el observador no vuelve a anunciar el mismo arranque. Reinicios diferentes tienen claves de entrega diferentes; una recuperación no borra el aviso anterior pendiente.
- `/api/monitor` conserva `cielo-monitor-v1` y añade una extensión `recovery_schema=1` cerrada: causa, supervisor BLE, fase, destino, duración, contador absoluto y ocho arranques recientes. Permite reconstruir varios reinicios entre consultas; si se exceden ocho, anuncia el hueco sin inventar detalles.
- Registro separado `restart_v2` en NVS, con checksum y capacidad fija. Una escritura al arrancar; enriquecimiento de hora/recuperación agrupado, máximo una vez cada 5 min. No cambia slots del perfil ni el historial antiguo. El bloqueo conserva su contexto en RTC antes del reinicio.
- Mensajes propios para recuperación: por ejemplo, «La orden de apagado de Pez Verde se ha completado por BLE». Causa de alimentación, OTA o software no se etiqueta como fallo BLE si no hay evidencia retenida.
- Resumen diario con reinicios, recuperaciones, incidencias y tramos sin consultas. Cero muestras se presenta como **SIN DATOS**. Distingue observador ausente de consultas que no alcanzan al ATOM y calcula días de 23/25 h con Europe/Madrid.

### P1: diagnosis y producto

- `skipped_busy` se conserva por compatibilidad, con su definición explícita: checkpoints ocupados, **no cambios de curva perdidos**. Se añaden denominadores `due_ticks`, ticks con operación en curso, pendientes sin operación, fase larga, evaluaciones y prioridad OFF. Estos contadores se solapan cuando corresponde; no son un porcentaje de fallo óptico.
- Informe Maestro único con `recent_restarts`, hora epoch (UTC), fase, dispositivo, duración y resultado de comunicación. `boot_epoch=0` significa hora aún desconocida. Los epochs se convierten a Europe/Madrid en el observador.
- Inicio y Centro mantienen tarjetas compactas y no convierten incidencias históricas en alarmas actuales. Alertas e historial muestra si el observador consulta recientemente y un solo botón de activación. El historial externo resume cobertura y presenta las últimas 100 entradas; exportación conserva el límite documentado de 1.000.

### P2: acceso LAN / OTA

- Sigue **sin claves de administrador u OTA, sin TLS, sin firma obligatoria y sin pulsación física**. No se reintroduce generación de certificados ni su consumo de memoria.
- Se añade validación del Host contra IP STA, IP AP o mDNS del propio equipo, además de Origin y Fetch-Site. Dificulta DNS rebinding y formularios de otros sitios sin añadir pasos al usuario. Las comprobaciones de chip S3/flash/aplicación, SHA-256, preparación y rollback se conservan.
- El AP protegido de recuperación no cambia. Esta edición no autentica a otro equipo ya dentro de la LAN; el acceso local confiable sigue siendo una elección explícita del propietario.

## Invariantes

Curvas V5, generación almacenada, PV34/CH31, RGB/brillos, motor, códecs, almacenamiento del perfil, paso `2/2/2→1/1/2`, apagado a 00:00, particiones y restauración `0x03` no se cambian. La única modificación deliberada de una cabecera antes fijada es la prioridad OFF opcional en `Scheduler::start_next`; su pin nuevo queda en `rc6_invariants_sha256.json`. El fixture RC5 original permanece para comparación.

## Validación y límites

Las suites host cubren callbacks ausentes, timeout de ambos destinos y rollover, OFF durante conexión/POWER ON/RGB con la función real, prioridad y fairness, journal lleno/corrupto, buffers del endpoint, primer reinicio y varios entre consultas, reinicio del observador, reintentos de entrega, datos ausentes, DST y actualización reversible. No acceden al ATOM ni envían Telegram. La compilación ESP32-S3 y su huella se validan por Actions; el manifiesto final indica el commit exacto.

**Pendiente en hardware:** contraste de ambas pantallas con el transporte nuevo, pérdida/retorno de cada controlador, medianoche y salida de luna, pausa/cesión My Chihiros/OTA y 24–72 h de estabilidad. Estos ensayos no pueden ejecutarse desde el contenedor. La causa raíz de los dos bloqueos anteriores permanece sin probar.

No se instala firmware ni se modifica el iMac a distancia. Para garantizar apagado ante una radio averiada hace falta un corte eléctrico independiente probado sobre la alimentación de PV; ningún envío BLE por sí solo puede garantizarlo.

## Actualizar el observador del iMac

El instalador Sierra V3 adaptado no está en este repositorio. No se presupone que su distribución coincida con `monitor/cielo_monitor`. `tools/update_observer.py` solo actualiza una instalación identificada del paquete, con el proceso detenido y su DB bloqueable; rechaza una distribución desconocida. Conserva configuración y credenciales, crea copia consistente de SQLite y backup del código, y no cambia el LaunchAgent.

```bash
python3.9 tools/update_observer.py "/ruta/que/contiene/cielo_monitor" --database "/ruta/real/history.sqlite" --check
python3.9 tools/update_observer.py "/ruta/que/contiene/cielo_monitor" --database "/ruta/real/history.sqlite"
```

Después se reinicia el LaunchAgent existente. Si es un script único V3, primero se debe revisar ese código local y adaptar el instalador; la herramienta se detiene sin alterar nada. Compatibilidad de sintaxis Python 3.9 y actualización probadas en host; ejecución real en macOS Sierra y certificados de Telegram pendientes de comprobar en ese iMac. No se requiere enviar el token.

Fuentes primarias del transporte:
- https://github.com/espressif/arduino-esp32/blob/2.0.17/libraries/BLE/src/BLEClient.cpp
- https://github.com/h2zero/NimBLE-Arduino/blob/2.3.6/src/NimBLEClient.cpp
- https://github.com/h2zero/NimBLE-Arduino/blob/2.3.6/src/nimble/nimble/host/src/ble_gattc.c
