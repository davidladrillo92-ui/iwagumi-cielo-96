# IWAGUMI CIELO · RC5, bloques 8 y 9

Candidata de banco del 21/10/2026. Base exacta: PR #33, commit
`4492f221f7d73cea796cb12b4acc0a819ff67486`. Versión nueva:
`11.1.0-RC5-B1-B9-MONITOR`. Incluye los bloques 1, 5, 3, 2, 4, 6 y 7
sin reiniciarlos ni sustituirlos. **Última instalación confirmada: RC3**.

## Qué implementa

| Necesidad | Implementación comprobable |
|---|---|
| Avisos al iPhone | Proveedor Telegram opt-in; requiere bot/chat y permisos de notificación en iPhone |
| Pantalla sin respuesta | Evaluación por lámpara, 3 muestras consecutivas; GATT antiguo >650 s cuando CIELO espera respuesta |
| Errores repetidos | Fallos consecutivos o incremento >=3 en ventana de 5 min; no alarma por contadores históricos |
| ATOMS3 desconectado | Observador en otro equipo: tres consultas fallidas; distingue falta de acceso de causa física desconocida |
| Recuperación | Dos muestras buenas, con estado persistente entre reinicios del observador; no recupera por datos ausentes o control cedido |
| Resumen diario | Día natural anterior en Europe/Madrid, hora 09:00 configurable; días DST de 23/25 h y cobertura incompleta explícitos |
| Historial 7/30 días | SQLite con retención 30 días; panel horario, últimos 1.000 eventos y exportación CSV de muestras completas |
| Telegram, MQTT, HA | Adaptadores opcionales fuera del ESP32; TLS, QoS1 y discovery HA de sensores sin canal de control |
| Antispam | Transiciones persistidas, nueva incidencia >=10 min, recordatorio >=1 h, recuperación solo si hubo aviso; bandeja de máximo 200 entregas y caducidad 24 h |
| Caída del observador/red doméstica | Heartbeat HTTPS externo opcional, configurable en un servicio independiente; no viene desplegado |
| Copia completa | Herramienta USB de dos lecturas exactas de 8.388.608 B, cifrado AES-GCM/Scrypt, SHA256 y validación de tabla |
| Inspección | Endpoint autenticado de particiones/NVS/heap; inspección offline del dump y listado SPIFFS opcional sin montar/formatear ATOM |
| Limpieza | Función `requestAuto()` sin llamadas y variables sin uso retiradas; rutas antiguas devuelven 410; eliminado flujo ZIP/CI antiguo y resultados obsoletos de esta rama |
| Modularización | Serializador portable, servicio monitor con permisos y módulo de inventario separados de main; observador dividido en transporte/estado/almacenamiento/entregas/panel |
| RAM/almacenamiento | Consulta externa <=2304 B de buffer (respuesta habitual <1 KiB) en lugar del Informe Maestro de reserva 22 KiB; histórico fuera de ESP32; sin nuevos registros periódicos en NVS |
| Bluetooth | Corregida reserva parcial de colas/tarea; limpieza completa ante fallo, reintento válido y un único worker; telemetría de mínimo de pila libre |
| Recuperación | Herramienta cifrada, vinculación al dispositivo original, simulación por defecto y autorización explícita antes de cualquier escritura |

## Límites que permanecen

- Código y pruebas automatizadas no son validación óptica ni prueba sobre el acuario.
- No hay ninguna copia física nueva de RC3: requiere USB del propietario. Tampoco se ha restaurado su ATOM.
- El observador necesita un host independiente encendido (Linux o macOS moderno con Python 3.11+).
  No obliga a comprar Home Assistant ni convierte Telegram o Internet en dependencia de iluminación.
  El iMac Sierra puede requerir otra máquina para el observador; no se presupone compatibilidad.
- Sin observador no hay alertas ni historia nueva. Sin Internet no llegan avisos Telegram.
  Un heartbeat externo puede avisar de la pérdida de la cadena de observación, no identificar su causa.
- El sistema no distingue por telemetría una lámpara apagada de una orden GATT aceptada.
- El nuevo BIN no está firmado con la clave privada original. No crear otra identidad para eludir OTA.
  CI entrega BIN candidato, fuente y manifiesto; la firma se aplica offline al BIN exacto con la clave existente.
- No se habilita `cielo.local`, no se cambia certificado, tabla de particiones ni clave pública OTA.
- Los dos slots NVS y las dos particiones OTA se conservan. No se monta SPIFFS ni se formatea memoria.
- La lectura del mínimo de pila es observación, no permiso para reducir la pila de 16 KiB sin ensayo.

## V5 protegida

El archivo `tests/fixtures/rc5_b7_immutable_sha256.json` fija SHA256 de referencias,
motor, codec, política nocturna, cola, almacenamiento y particiones respecto a la base.
La suite falla si cambian. Referencia V5: PV34/CH31, FNV-1a32 `5814359c`;
22:59 2/2/2 → 23:00 1/1/2, sin punto lunar 1/1/1. No autoimporta el perfil:
el Informe Maestro 40 conserva evidencia de 34/32 en RC3.

## Verificación reproducible

```sh
python3 -m pip install cryptography==46.0.4 paho-mqtt==2.1.0 platformio==6.1.18
python3 tools/check_b89.py
python3 -m platformio run -e atoms3_lite_c124
```

Incluye suites anteriores B1–B7, políticas C++ y UI JavaScript, monitor con
reloj simulado, SQLite, TLS local real (certificado correcto/equivocado antes de enviar
credencial), tamaño/redirect/esquema malformado, historiales, DST, ausencia,
recuperación, ventana de errores, deduplicación, entrega/reintento y concurrencia
al sustituir un mensaje pendiente. Telegram y MQTT usan dobles sin enviar avisos.
Pruebas USB simulan lecturas, fallo de checksum y cifrado; FreeRTOS simulado
falla cada reserva en la función real de inicialización. No se anuncian como ensayos físicos.

## Antes de una instalación

1. Contingencia eléctrica independiente solo para Pez Verde y respaldo RC3 real cifrado.
2. Exportación e Informe Maestro actuales; inventario real y recuperación USB disponibles.
3. Firma offline del BIN exacto y comprobación contra la pública integrada; OTA conserva sus controles.
4. Safari/HTTPS, BLE y memoria TLS+Wi-Fi+dos lámparas, POWER OFF físico, retorno 0x03,
   rollback y estabilidad 24–72 h supervisada. Validar también token monitor sin permiso de escritura.
5. Activar observador y probar avisos en el iPhone del propietario de forma explícita.

No instalar esta candidata para dejarla sin supervisión el viernes 23. Si la protección
externa no está probada, mantener Pez Verde desconectada durante la ausencia.

## Documentación

- [Observador y despliegue](../monitor/README.md)
- [Respaldo e inventario USB](USB_RECUPERACION.md)
- [Auditoría de cambios y recursos](MANTENIMIENTO.md)

Referencias primarias consultadas: [esptool v4 ESP32-S3](https://docs.espressif.com/projects/esptool/en/release-v4/esp32s3/esptool/basic-commands.html),
[Telegram Bot API](https://core.telegram.org/bots/api#sendmessage),
[Paho MQTT 2](https://eclipse.dev/paho/files/paho.mqtt.python/html/client.html),
[MQTT Home Assistant](https://www.home-assistant.io/integrations/mqtt/).
