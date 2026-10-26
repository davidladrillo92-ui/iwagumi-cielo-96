# CIELO RECOVERY3.3 — auditoría Informe Maestro 71, 26/10/2026

## Evidencia
Firmware instalado al recoger el informe: RECOVERY3.1-CANDIDATE, boot 126, 6131 s. Arranques 125 y 126 por bloqueos de 120004 ms (Pez Verde) y 120002 ms (Chihiros), fase reportada CONEXION. Boot 126: PV 100/100 y CH 100/100 GATT; 100 y 101 conexiones físicas, respectivamente; Wi-Fi 0 caídas, RSSI -36 dBm. El contador de 180 fallos persistentes es histórico, no 180 fallos de este arranque.

## Comparativa real de código
- SIMPLE1 reutilizaba conexiones hasta 90 s de inactividad. **RC3 ya liberaba la conexión después de cada lote** (5 s de inactividad adicional); por tanto, no es exacto decir que todas las versiones antiguas las retenían.
- RECOVERY3.1 liberaba explícitamente cada conexión tras cada lote; con actualizaciones/heartbeats de 60 s se producían muchas aperturas/cierres.
- PV OFF tras GATT aceptado pasó de 300 s a 60 s; RECOVERY3.3 recupera 300 s tras éxito, conservando el backoff de fallo y el OFF prioritario.
- La conexión Bluedroid parcheada tiene 8 s de espera nominal REG/OPEN; los 120 s de supervisor no prueban que el tiempo de conexión sea siempre acotado. Pueden existir llamadas SDK/semáforos o callbacks pendientes. Se añaden etapas de diagnóstico pasivo, **sin destruir clientes SDK bloqueados**.

## Cambios propuestos (no instalados)
1. `ble_keep_links(pv,ch)` ahora controla retención real; PV y CH conservan enlace mientras CIELO es propietario. La tarea BLE sigue siendo única propietaria de las llamadas GATT. OTA, rescate y devolución a My Chihiros liberan enlaces desde la tarea BLE. Los enlaces fallidos se liberan cuando no haya limpieza SDK pendiente.
2. `onDisconnect` distingue liberación intencional de pérdida inesperada. Se informa contador y tiempo por lámpara, junto con estado REG/OPEN del SDK y último código ESP.
3. En pérdida inesperada de cualquier enlace: se solicita OFF prioritario para PV; en horario diurno no se reanuda RGB hasta obtener GATT OFF aceptado. Para CH, si estaba bajo control CIELO, se solicita el selector personalizado 0x03 y se libera el enlace; no se envía 0x05 (escenas no verificadas). Si BLE no funciona, **ninguna orden puede garantizar un apagado físico ni el retorno óptico**.
4. OFF de PV confirmado vuelve a comprobación cada 300 s, no 60 s. Los reintentos de fallo y la protección de apagado siguen activos. Los cambios de curvas siguen en cadencia 30 s; heartbeat RGB 60 s sin cambio permanece por ahora, para observar transporte.
5. Se conservan curvas V5 (PV 34 puntos; CH 31), colores, hora, monitor Telegram, app y rollback.

## Riesgos, condiciones de prueba y límites
- Mantener la conexión CH mientras CIELO la controla puede impedir que My Chihiros se conecte simultáneamente. Para cederla hay que salir del control CIELO/usar la devolución prevista.
- Desconexión involuntaria: el firmware solo puede intentar el retorno CH 0x03 y OFF PV cuando BLE vuelva a estar operativo. El usuario debe comprobar físicamente la reacción de ambas pantallas. No hay relé eléctrico ni sensor óptico.
- `m_cieloStage` solo permite observar el último tramo REG/OPEN/espera, no constituye una reparación del SDK. La conexión sigue pudiendo quedarse bloqueada.
- Pruebas obligatorias en horario diurno: conexión persistente 2 h, modificaciones de curva, My Chihiros liberado manualmente, desconexión real de cada lámpara por separado, reinicio ATOM, OFF a las 23:00 y estabilidad nocturna con observador Sierra/Telegram y rollback disponible. Si se repiten bloqueos, investigar el SDK y considerar NimBLE en rama experimental, sin cambiarlo a ciegas.
- Comparar **bloqueos por 100 conexiones** y **por 24 h** frente a RECOVERY3.1; un arranque sano de 100 operaciones no prueba mejora estadística.
- No fusionar ni instalar automáticamente; candidato experimental, sin validación física.
