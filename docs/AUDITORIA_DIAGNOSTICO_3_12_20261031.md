# Auditoría y desarrollo RECOVERY3.12 — diagnóstico verificable

31/10/2026, Europe/Madrid. Base exacta: RECOVERY3.11, fuente GitHub `e8868093205baee7a852c79bffe51d204b557e05`, PR #64. La nueva candidata no está instalada. El trabajo no accede al ATOM, al iMac ni al bot Telegram.

## Evidencia y contraste con GitHub

Se han leído Issue #35 (plan tras bloqueos de 120 s), Issue #38 y sus cinco comentarios de investigación/implementación, Issue #20 (auditoría de producto), PR #64 y las auditorías incluidas de 3.9, 3.10 y 3.11. Se contrastan los Informes Maestros privados 92–95; solo se publica una selección técnica sin credenciales en `DIAGNOSTIC_EVIDENCE_92_95_20261031.json`.

| Informe | Actividad del mismo boot142 | GATT PV/CH correctos | Guardados LittleFS verificados | Perfil |
|---|---:|---:|---:|---|
|92,27/10 16:29|5064s|83/82|0|gen10,38PV/32CH|
|93,28/10 08:32|62838s|637/1046|2|gen12,35PV/43CH|
|94,29/10 16:07|176499s|1597/2940|3|gen13,35PV/44CH|
|95,31/10 14:08|342157s|3127/5753|4|gen14,37PV/44CH|

Los cuatro informes conservan dos avisos blandos del arranque y 184 incidencias históricas, sin nuevos errores BLE, reinicios ni desconexiones registrados. Informe95: mantenimiento iniciado12:21, subida10min, retorno13:31–13:33 completado; amanecer PV14:00. Es evidencia favorable de95h, no garantía frente a averías futuras ni prueba óptica. No certifica por número de puntos la meseta65/60/56 del último JSON.

## Hallazgos y correcciones implementadas

1. **RGB antiguo después del apagado.** `prevPv` conserva la última orden RGB normal, mientras la ruta prioritaria envía OFF sin actualizar ese caché. Informe93 podía mostrar1% a08:32 con OFF aceptado. Se conserva el dato histórico por compatibilidad y se etiqueta `LAST_NON_PRIORITY_RGB_SUBMISSION`. Se añaden `pv_effective_requested` y su fuente: cero cuando existe obligación de OFF bajo control CIELO; en otro caso, la consigna de control actual. No se denomina medición ni confirmación de aplicación física.
2. **NVS vs LittleFS.** `lastCurveTransaction` representa exclusivamente la transacción NVS antigua: su `verified=false` no describe el guardado LittleFS. Nuevo `diagnostic_evidence.curve_save` identifica backend y resultado: NOT_ATTEMPTED_THIS_BOOT, UNCHANGED, VERIFIED, FAILED o UNCERTAIN. La verificación no realizada es null; un no-op no implica nueva escritura. El bloque legado NVS declara su backend y aplica null a su verificación cuando el backend FS está listo. No se cambia ninguna transacción de guardado.
3. **Identificar el perfil exacto.** El mismo checksum FNV1a32 que utiliza el importador se calcula a partir de los puntos activos, sin generación/metadatos. Está en status y Maestro, junto con validez del contenido y generación. Permitirá comprobar si el perfil importado es exactamente el esperado sin inferirlo de35/37/44 puntos. No autentica el origen ni es una medición óptica.
4. **`skipped_busy` y denominadores.** El código incrementa el contador cuando observa ocupación al llegar un checkpoint, pero continúa evaluando la programación. Informe95:355/11694 checkpoints ocupados (3,03%),11693 evaluados y uno inhibido. Se publican nombres y denominadores explícitos, porcentaje en puntos básicos y `missed_updates:null`/`missed_updates_measured:false`. La evidencia no permite afirmar cuántas actualizaciones ópticas se perdieron. No se modifica el scheduler.
5. **p95 y memoria.** El p95 existente se calcula sobre un ring de32 operaciones, aunque `latency_samples` acumulaba miles. Se añade la ventana efectiva y se conserva el mínimo10 del contrato actual. El mínimo de bloque libre se muestrea cada15min y acumula desde boot: se declara esa ventana, sin presentarlo como mínimo de los últimos15min ni diagnóstico de fuga. No se altera la política de latencia ni muestreo.
6. **Historial útil de mantenimiento.** Los36 eventos RAM generales se llenan con OFF normales y borran pronto la sesión anterior. Nuevo ring RAM de16 eventos operativos con códigos de lista cerrada, uptime y epoch capturado solo si el reloj ya es confiable. Conserva comprobación, inicio, listo, retorno, fin, cancelación y guardados; OFF repetidos no lo desplazan. No guarda detalles arbitrarios, SSID, claves, MAC ni IP; no escribe en flash y se pierde al reiniciar. No reconstruye fechas históricas con el reloj actual.
7. **Filesystem y muestras día/noche.** Se informa si el backend de curvas está montado/listo, aclarando que no es un chequeo completo del filesystem. El historial de96 muestras y los contadores acumulados tienen ventanas distintas: se declaran ambas. La clasificación día/noche antigua usa14:30–23:00 fijos; se etiqueta como tal y no como estado luminoso del perfil.

Los campos comunes se generan con un único serializador `diagnosticEvidenceJson()` para `/api/status` y el Informe Maestro. El endpoint compacto `/api/monitor` y el vigilante Sierra no cambian.

## Conservación y comprobaciones

La suite comprueba por hashes los rangos del motor, BLE, desconexión, reconexión, supervisor y mantenimiento3.8, además de almacenamiento LittleFS y particiones. Se conserva la pila loop16KiB. No se modifican curvas instaladas, protocolos, tiempos8/30/180/300s, acceso LAN, OTA ni credenciales. La interfaz visual3.11 se conserva byte a byte. Los nuevos helpers son de lectura y el historial ocupa RAM fija; la captura de eventos no añade llamadas NVS ni archivos.

Se ejecutan pruebas C++ de clasificación, denominadores, checksum y retención, además del serializador de producción con fixtures FS/NVS, ningún intento, no-op, error y publicación incierta; se ensaya que un éxito NVS ya verificado no sea llamado incierto. Se comprueba el OFF efectivo frente al caché RGB antiguo. La suite completa conserva pruebas del vigilante, almacenamiento, motor, interfaz y handlers reales. CI compila ESP32-S3, revisa pila y ejecuta navegador y Sierra/Python3.9. Los resultados concretos de compilación se añaden después de obtenerlos; no se confunde una prueba host con ensayo físico.

## Pendientes separados y orden de implementación

| Prioridad | Pendiente | Estado/criterio |
|---|---|---|
|P1|Validar3.12 en hardware|Comparar boot/perfil/checksum, OFF nocturno, amanecer y mantenimiento; confirmar que el nuevo resumen coincide con la telemetría. Conservar3.11 para retorno.|
|P1|Vigilante instalado y Telegram|La fuente contiene detección del primer reinicio, pero no demuestra que la V3/V4 instalada coincida. Verificar paquete/versión real, arranque automático, certificados, entrega real y períodos sin datos; no publicar token ni sustituir sin identificar la instalación.|
|P1|Guardado Wi-Fi atómico|Informes92–95 siguen LEGACY_READ_ONLY y0 escrituras: implementación host/CI probada, validación física de guardar/repetir/reiniciar pendiente. No es necesario cambiar Wi-Fi para usar esta versión.|
|P1|Fallos inducidos BLE y callback tardío|Los95h sin fallos no ejercitan recuperación tras avería. Ensayar pérdida real de ambos controladores, regreso y OFF, estando el propietario presente; registrar fase/SDK/worker separados. No se da por demostrada la causa de los120s originales.|
|P2|Reducir esperas de arranque|Persisten30s BLE y180s de validación. Medir subfases en banco y modificar una sola política; conservar validación OTA y OFF inicial. No incluir cambios de recuperación en esta candidata de diagnóstico.|
|P2|Clasificación día/noche dependiente del perfil|Los contadores antiguos tienen horario fijo. Proponer contadores nuevos con definición de iluminación programada, mantenimiento y control manual; no reinterpretar muestras antiguas.|
|P2|Seguridad LAN/OTA y acceso externo|Sigue NONE_LAN/HTTP y OTA sin firma obligatoria según decisiones previas. Sesión/pairing/TLS requieren diseño y pruebas separadas; no abrir puertos del router.|
|P2|App instalada iPhone|Manifest, iconos y standalone ya existen. Probar importación/exportación, reanudación tras segundo plano y estados sin ATOM desde pantalla de inicio; no supone control remoto ni vigilancia en segundo plano.|

No se cierran las issues históricas como si todas sus condiciones físicas estuviesen cumplidas. La mejora demostrada por los informes corresponde a3.11; la implementación3.12 aporta diagnósticos nuevos y requiere su propia validación.
