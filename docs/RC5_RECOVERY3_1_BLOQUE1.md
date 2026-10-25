# RECOVERY3.1 — Bloque 1: clasificar bloqueos BLE

## Evidencia
El 25/10/2026, arranque 122 a 123, se notificó Chihiros / CONEXION / 120000 ms. El snapshot de recuperación anterior mezclaba edad del worker con edad de conexión SDK pendiente. Por tanto, esa etiqueta no identifica con certeza una llamada connect bloqueada.

## Cambios de diagnóstico
- Captura independiente de fase y heartbeat reales del worker, y edades de limpieza de conexión pendientes para Pez Verde y Chihiros.
- Clasificación de solo lectura: SDK_CONNECT_PENDING, GATT_DISCOVERY_WAIT, WORKER_UNRESPONSIVE o NONE.
- Las banderas pueden coexistir; la fase de descubrimiento es inferida, no prueba de callback perdido.
- Informe Maestro: bluetooth.blockage_diagnostic; registro de intento de rescate: BLE_REINICIO_DIAGNOSTICO.
- Umbral diagnóstico 15 segundos. No cambia el límite de rescate de 120 segundos.

## Límites
La edad de solicitud SDK es aproximada porque pendingSince se actualiza durante reconciliación. No se cambia aún la espera GATT en getServices, el algoritmo de rescate, el interlock OFF, los protocolos ni las curvas. Los eventos anteriores al reinicio no tienen todavía persistencia garantizada. Las pruebas de host no demuestran funcionamiento de radio real.

## Pruebas
g++ -std=c++11 -Iinclude tests/test_ble_blockage.cpp -o /tmp/cielo_ble_blockage_test && /tmp/cielo_ble_blockage_test

No instalar un nuevo BIN hasta pasar compilación y validación física.
