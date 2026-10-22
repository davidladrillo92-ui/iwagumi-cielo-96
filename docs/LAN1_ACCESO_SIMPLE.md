# RC5 LAN1: uso local sin contraseñas

Candidata: `11.1.0-RC5-B1-B9-LAN1`, ATOMS3 Lite C124, ESP32-S3, flash 8 MiB.
Decisión expresa del propietario: sustituir los requisitos de HTTPS, contraseña de administrador, clave OTA, firma ECDSA y autorización física por acceso directo en su wifi. Las instrucciones de RC4, RECOVERY y STACKFIX1 sobre estos requisitos son históricas y no se aplican a LAN1.

## Qué estaba pasando

- RECOVERY se recibió y llegó a ejecutar el código nuevo; falló por desbordamiento de `loopTask`. El retorno a RC3 funcionó al tercer arranque.
- STACKFIX1 arrancó con 32768 B y mínimo libre medido de 22656 B después de inicializar HTTPS. Más tarde, el registro mostró repetidamente `mbedtls_ssl_setup -0x7F00` (fallo de asignación de memoria). No es evidencia de una contraseña incorrecta. `AUTH_RECHAZADA` es un evento distinto de credencial rechazada.
- Además, el código posarranque reiniciaba su marca de tiempo durante los quince segundos de espera. Esto impedía completar la validación y podía provocar retorno a RC3 a los diez minutos. El último estado facilitado por el usuario confirma RC3, aunque ese JSON por sí solo no indica la causa del retorno.
- LAN1 no arranca el servidor HTTPS, no provisiona certificados, no deriva claves PBKDF2 y no reserva tareas/buffers de conexiones TLS. Conserva un `loopTask` explícito de 16384 B; la memoria libre real se comprueba por USB/Informe Maestro en el ATOM. No se promete un margen físico que aún no se ha medido.
- La validación usa un contador de salud continua: quince segundos sin perder Wi-Fi, coexistencia, BLE, reloj, perfil ni los GATT exigidos. Un fallo reinicia la espera. La persistencia de éxito se completa antes de dar la candidata por validada.

## Uso después de instalar

Abrir `http://192.168.0.25/` o `http://iwagumi-cielo96.local/`. Usar **http**, no el enlace HTTPS guardado de la versión anterior. Centro muestra acceso local; no existen campos de contraseña ni firma. El Informe Maestro completo y mantenimiento se consultan directamente. La contraseña de la red wifi y el respaldo cifrado existente conservan su función; no son claves de acceso a CIELO.

Actualizaciones posteriores: Centro → Actualizaciones → seleccionar el BIN → Instalar → confirmar. La aplicación calcula SHA-256 automáticamente, prepara las luces y transmite el archivo. No se requiere pulsar el ATOM. Se mantienen los controles del hardware, no de identidad del autor:

1. BIN de aplicación, 500000 B como mínimo y contenido dentro de la partición inactiva.
2. Cabecera ESP, chip ESP32-S3 (id 9), flash declarada de 8 MiB, descriptor de aplicación válido. Rechaza bootloader, ZIP y dump completo. La cabecera se reúne incluso si llega dividida entre fragmentos; no se escribe antes de aceptarla.
3. SHA-256 idéntico al calculado por el navegador y validación final del actualizador ESP.
4. Preparación BLE, timeout de carga y partición alternativa. Retorno por arranque no validado: tres intentos o diez minutos cuando el programa puede ejecutarlo.

La compatibilidad de formato no demuestra que otro programa vaya a controlar estas luces ni que su funcionamiento físico sea correcto. Usar el BIN de aplicación de esta candidata para el ensayo del proyecto.

## Primer cambio desde RC3

RC3 sigue siendo el programa instalado y conserva sus requisitos originales. Este cambio no puede desactivar una clave en el dispositivo antes de instalarse. Desde RC3 usar su procedimiento que ya permitió cargar STACKFIX1, con la clave antigua y el permiso físico **solo para esta migración**. No configurar una nueva clave. Elegir `IWAGUMI_CIELO_RC5_LAN1.bin`, no RECOVERY ni STACKFIX1. Si RC3 no permite el acceso con la clave que ya estaba configurada, detener ese método: conservar el respaldo y planificar instalación USB con inspección de partición activa antes de escribir.

No borrar toda la flash ni NVS. La candidata preserva perfiles V5, wifi y particiones. Los hashes antiguos de autenticación y la identidad TLS guardada quedan sin uso en NVS para permitir el retorno a RC3; no se exige conocerlos en LAN1 y no se extraen ni publican. Unos pocos valores sin uso no prueban que exista basura capaz de explicar el bloqueo BLE.

## Ensayo supervisado

- Confirmar respaldo completo de 8388608 B mediante la comprobación de la herramienta de backup, no únicamente por existir un archivo cifrado.
- Antes del cambio, tener control eléctrico independiente de Pez Verde. El filtro y el calentador mantienen alimentación.
- Después del reinicio comprobar versión LAN1 por `/api/service/status` y obtener Informe Maestro alrededor de los minutos 4 y 12.
- Verificar `postboot_candidate_pending=false` y evento `OTA_POSTARRANQUE_OK`; Wi-Fi, reloj, V5 PV34/CH31, fase/heartbeat BLE y memoria. Revisar ambos estados físicos de luz, especialmente POWER OFF Pez Verde y retorno My Chihiros. GATT no sustituye esta observación.
- Abrir/cerrar la web varias veces y probar las acciones normales. No deben aparecer errores mbedTLS porque no se inicia TLS en LAN1. Comprobar que no vuelve a RC3 tras diez minutos.
- La recuperación del bloqueo BLE de Informe 41 sigue integrada: reinicio supervisado al detectar fase/heartbeat atascado, presupuesto de reinicios y obligación inicial de OFF Pez Verde. Requiere ensayo físico; no garantiza apagado eléctrico si BLE falla.
- Pruebas nocturnas, OTA desde LAN1 a otra candidata y rollback físico siguen pendientes. Mantener draft hasta completarlas. No dejar la candidata sin supervisión durante la ausencia del viernes.

## Observador B8 sin claves de CIELO

`monitor/config.example.json` selecciona `local_lan: true` y HTTP local. No requiere `certificate_sha256`, `CIELO_MONITOR_TOKEN` ni `CIELO_PANEL_TOKEN`. El panel enlaza únicamente `127.0.0.1:8765` y se abre sin clave por defecto. El modo anterior de TLS fijado permanece opcional para equipos que sí lo utilicen; no es un requisito de esta candidata.

Desde un ordenador compatible con Python 3.11: instalar `monitor/requirements.txt`, copiar la configuración y ejecutar `python -m cielo_monitor --config config.json --check` desde `monitor/`; después iniciar sin `--check` para guardar histórico. Telegram/MQTT necesitan sus propias credenciales de proveedor si se habilitan. No se ha desplegado el observador ni enviado avisos desde este desarrollo. Para detectar un ATOM sin corriente se requiere ese ordenador independiente; para detectar también la caída del observador se necesita el heartbeat externo opcional. El historial de 7/30 días empieza a existir cuando el observador funciona; no se inventan datos anteriores.

## Conservación de fuentes

PR #34 contiene las fuentes completas y las pruebas. CI genera BIN de aplicación, ZIP de fuentes, manifiesto con SHA-256, tabla de particiones, herramientas USB y símbolos ELF/mapa. Los colores, interpolación, códecs BLE y particiones se comparan con sus huellas de referencia inmutables.
