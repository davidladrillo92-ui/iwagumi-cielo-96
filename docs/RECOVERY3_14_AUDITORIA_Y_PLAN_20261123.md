# RECOVERY 3.14 — Informe de ingeniería y plan de aceptación (23/11/2026)

**Estado: candidata de desarrollo, no instalada ni certificada en hardware.** Base 3.13 `3acc5b6` (ejecutándose en el ATOM real). Perfil LittleFS activo V5 generación 22, checksum de exportación FNV1A32 `c1960ba7`: Pez Verde 37 puntos, Chihiros 46 puntos. Ni el perfil, ni motor RGB, ni archivos LittleFS, ni el mapa de particiones se modifican. 3.13 permanece como referencia de recuperación. Monitor Mac eliminado en 3.13, no se restaura.

## Evidencia de producción (Informe Maestro boot150, 23/11/2026 18:17:28)

PV operaciones GATT 9/9, CH 8/8; error GATT 0. Tiempo máximo de operación PV 5.785ms, CH 6.722ms, pero fase de conexión solo 121ms/39ms y escritura individual 3ms/146ms. La exploración `BLEScan::start(5,false)` esperaba hasta cinco segundos aunque el anuncio pudiera recibirse antes: esta es una **causa probable**, no una observación de fase que se hubiera medido por separado en 3.13. Recuperación de curvas tras ~180s porque se condicionaba a `currentBootMarkedHealthy`, que debe seguir siendo independiente del control de luces. GATT correcto no implica respuesta óptica.

## Implementación en la rama 3.14

### Inicio BLE rápido y robusto

- Descubrimiento **2 segundos primero**, con reintento original **5 segundos si no se encuentra el dispositivo**; los dispositivos localizados siguen reutilizando dirección y conexión. Ventaja potencial: ahorrar ~3 segundos en primer enlace. Peor caso con dos escaneos: hasta 7 segundos; la fiabilidad se preserva sin descartar la ventana completa de 5 segundos. Se registran los tiempos reales de descubrimiento por dispositivo.
- Tiempos de conexión, servicio, característica, escritura y pausas disponibles en `operation_breakdown_last`; se registran la parte no atribuida y eventos `BLE_LENTO_DESCUBRIMIENTO` separados de lentitud posterior. Todavía no existe un percentil 95 fiable en el primer arranque, requiere muestra suficiente.
- Inicialización BLE tras 30 segundos si el AP privado y la coexistencia Wi-Fi/BLE están disponibles **sin exigir conexión del router doméstico**. Reintentos de creación de tarea BLE acotados a tres, con intervalo. Reconexión de STA limitada con backoff 30–120 segundos y evitada mientras hay trabajo BLE.

### Recuperación de iluminación protegida

- Posibilidad de reanudación de ambas curvas a partir de 45s cuando el reloj es de confianza, el perfil íntegro, BLE vivo, la cola libre y no hay OTA, rescate, laboratorio, mantenimiento, retorno CH o problemas de watchdog.
- En Pez Verde la obligación y confirmación GATT de `POWER OFF` conservan prioridad. **Nunca se interpreta una escritura GATT como prueba óptica.** Durante la noche no se permite encender PV; durante el día se exige desbloquear previamente interbloqueos OFF.
- Se conserva la observación de arranque OTA de 180s y la ventana de rollback de 600s. El estado `currentBootMarkedHealthy` no se adelanta; la reanudación de iluminación tiene condiciones propias.
- `boot_timing` en Maestro: inicio BLE, primer GATT por lámpara, luz reanudada, boot considerado sano (ms desde arranque, actual boot, nullable).

### Reloj y funcionamiento sin Internet

- Corrección de `POST /api/clock`: acepta 10–11 dígitos numéricos para epoch UNIX permitido [1700000000,4102444800], evitando el error lógico anterior.
- NTP o sincronización explícita del móvil aportan confianza. Existe testigo limitado de reloj en **RTC RAM**, únicamente válido tras reinicio de software, con checksum y ventana <=900s. No se escribe flash. Tras pérdida de corriente sin NTP ni sincronización desde iPhone, el sistema debe **mantener la programación apagada** mientras no disponga de hora fiable: no se puede reconstruir la hora absoluta desde un ESP32-S3 sin batería RTC.
- Los indicadores del Informe Maestro distinguen `NTP`, `PHONE`, `RTC_SOFTWARE_RESTART`, `UNTRUSTED`.

### Respuestas tardías de Chihiros

- Cada solicitud `0x03` tiene ID de 32 bits enviado a la tarea BLE; el resultado conserva ese identificador. Un callback anterior a un timeout o perteneciente a otro intento se ignora y cuenta en `stale_custom_resume_responses`. Se mantienen pausas y acciones de retorno voluntario del propietario.

### Diagnóstico/historial y memoria

- `daily_monitoring.daily_30`: hasta 30 días de métricas agregadas, en RAM acotada. Advertencia explícita `persisted:false`, histórico **se pierde al reiniciar**; no se añaden escrituras a NVS/LittleFS ni se alteran curvas.
- Reserva inicial del JSON Maestro ajustada para reducir reallocaciones; **no está realizado** todavía un streaming completo byte a byte de grandes respuestas. Esa refactorización no se incluye en 3.14 por su impacto en rutas de lectura/autorización y memoria y requiere pruebas de equivalencia propias.
- HTTPS/autenticación LAN no se rediseñan en esta rama (herencia 3.13). AP y ruta de hora conservan la política de acceso existente; no exponer CIELO directamente a Internet.

## Verificación automática

- `python tools/check_b89.py`: los hashes congelados de motor, BLE, mantenimiento y storage siguen aplicando. Solo se normalizan explícitamente los cambios autorizados, no se desactiva el control de integridad.
- `python tools/check_littlefs_migration_lab.py`: cortes, generación, snapshots, sin formato automático.
- `tests/test_recovery314.cpp`: 45s, hora no fiable, gate PV OFF, rescate/OTA/laboratorio, AP sin router, retries y stale reply ID.
- `tests/test_history314.cpp`: límite de 30 días, repetición del mismo día, retroceso de hora, memoria.
- Compilación real PlatformIO ATOMS3 Lite C124 y auditoría UI en GitHub Actions. La evidencia definitiva corresponde al workflow del commit actual, no a builds de commits anteriores.

## Pruebas de aceptación física pendientes

1. Al arrancar con luz diurna, medir desde alimentación hasta PV visible y CH visible, y comprobar por observación que el primer POWER OFF PV sucede antes que el RGB diurno cuando proceda.
2. Arranque en horario nocturno: PV apagada; CH desde la curva aprobada y apagado final; comprobar con ojos, no solo GATT.
3. Sin router: AP privado + radio BLE; sincronizar reloj desde teléfono y comprobar que la iluminación funciona, si no hay reloj fiable debe permanecer en modo seguro.
4. Retorno de Chihiros a MyChihiros, retrasos/reintentos y callbacks tardíos simulados; ausencia de comandos de color durante cesión de propiedad.
5. Conectar/retirar STA durante funcionamiento; comprobar que no se pierden luces y que los intentos tienen backoff.
6. 24–48 h de carga real y reporte Maestro: heap, eventos, tasa de fallos, conservado perfil `gen22/c1960ba7`. Mantener BIN 3.13 de recuperación.
7. Fallo OTA y rollback sobre hardware de prueba, sin provocar corrupción del acuario operativo.

**No fusionar PR #69 ni instalar en el ATOM de producción sin validar las pruebas físicas y contrastar informes Maestro.**
