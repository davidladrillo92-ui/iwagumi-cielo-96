# B9 · Auditoría de consolidación

## Cambios deliberadamente acotados

- `cielo_monitor.cpp`: serialización de esquema mínimo, portable y con límite de capacidad.
- `cielo_monitor_service.cpp`: token de lectura independiente, hash en NVS solo al crear/revocar;
  no sesiones administradoras en observador ni escrituras periódicas de histórico en flash.
- `cielo_maintenance.cpp`: lectura de tabla real, NVS y memoria. No monta ni formatea SPIFFS.
- `main.cpp`: adaptador de snapshot que lee estado existente; no duplica scheduler ni worker.
- `ble_begin()`: antes usaba la existencia de la primera cola como prueba de inicialización.
  Un fallo de reserva o creación de tarea dejaba colas vivas y podía informar éxito falso
  al reintentar. Ahora solo un handle de tarea válido acredita inicio; cualquier fallo previo
  libera todas las colas. Los tamaños y prioridades se mantienen.
- `releaseTarget()`: quitada una asignación duplicada e inmediata de la misma fase.
- Función muerta `requestAuto()` y variables sin uso `cieloLastNightSample`, `otaRestarts`
  retiradas. AUTO inseguro continúa bloqueado en el worker; no se activa ninguna escena nueva.
- Rutas históricas sin implementación: respuesta 410 clara. No se redirigen a operaciones nuevas.
- Flujo `build-from-zip.yml`, ZIP antiguo y `ci-results/latest.*` retirados de esta rama:
  eran otra fuente de compilación y podían republicar código viejo en main. El historial Git
  los conserva. El flujo actual solo lee el repo y publica artefactos; permisos `contents: read`.
- HTML es la fuente del C++ embebido. Test de igualdad exacta obligatorio.

## Recursos y puntos de observación

Colas BLE: 2 órdenes, 4 resultados, solicitudes de scan/probe de 1, resultados de 2,
liberaciones de 2, retorno personalizado de 1 y resultado de 1. Tarea 16.384 bytes,
prioridad 1, núcleo 0. No se reducen buffers o pila por estimación. `ble_stack_free_bytes`
expone la marca mínima libre (unidad bytes en ESP-IDF), además del heap mínimo y mayor bloque.

El histórico largo y las entregas están fuera del ATOM. No se añade tarea saliente HTTPS,
MQTT, Telegram, DNS ni cliente cloud a su bucle. El monitor recibe una consulta normal/minuto,
con máximo una cada 15 s. TLS continúa consumiendo recursos: medir la coexistencia real.
El buffer estático del serializador es de 2304 B; no se afirma una reducción global del heap
sin comparar mediciones reales. El crecimiento de flash no equivale a consumo de RAM.

SQLite retiene 30 días y reutiliza páginas liberadas; máximo 200 entregas, 1.000 eventos
por vista y 16 puntos para ventana de errores. El tamaño de archivo puede conservar su
máximo anterior; no se ejecuta `VACUUM` masivo periódico ni se promete que borre datos
físicamente. Se recomienda disco cifrado del observador.

## Fronteras de compatibilidad

Inmutables por hash: V5 exacta, referencias, motor/interpolación, codec BLE, política de
apagado, cola, ABI NVS, particiones y clave pública de firma. No borrar `curvesA/B`, claves
TLS/OTA ni `otadata`. Mantener PR en borrador. El firmware actual instalado sigue RC3.

Pruebas incluidas fuerzan fallos de cada reserva BLE y de creación de tarea con FreeRTOS
simulado. Validación sobre ESP32, mínimo real de pila, pérdidas de alimentación, dump USB,
rollback y noche supervisada siguen como pruebas físicas pendientes, no como fallos ocultos.
