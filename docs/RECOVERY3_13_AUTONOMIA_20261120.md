# RECOVERY 3.13 — autonomía local, revisión del informe de larga duración

Base exacta: `c2cb7fedcc0cc887f003f0c84bcac0a0b8748f39` (3.12). Informe del 20/11/2026 a las 18:38:45, reloj Europe/Madrid: boot 149, 672727 s, 5644 operaciones PV y 11205 CH GATT correctas, un timeout blando PV, ninguna operación BLE fallida. LittleFS gen22, checksum c1960ba7, 37/46 puntos; copias NVS gen9/8 válidas pero antiguas. NVS 156 libres / 630, reserva anterior 188, 2244 guardados diagnósticos omitidos. Ningún dato demuestra respuesta óptica. No se publica el Informe Maestro privado ni direcciones/credenciales.

## Alcance de esta candidata

- Chihiros: pérdida inesperada arma reconexión si autonomía activada; conserva retorno 0x03, pausa voluntaria, separación de PV, supervisión y backoff existentes. Sólo una respuesta de la cola coincidente y en control activo cancela la recuperación; un resultado tardío durante el retorno no debe cancelarla.
- Estado de salud autónomo: fallos esperados por lámpara después de observación inicial de 600 s, excluyendo pausa/laboratorio/OTA/rescate. OFF programado PV se considera reciente hasta 300 s + 15 s de margen, sólo con enlace y sin error/interbloqueo. No implica luz medida. Se exponen flags por lámpara en status y Maestro.
- NVS: con LittleFS montado se reserva una página GC (126 entradas), margen crítico (12) y 384 bytes de diario (14 entradas): 152 en vez de 188. La cifra es un límite inferior, no garantía física; si falta espacio/SDK rechaza, permanece pendiente y el intento se limita a 5 minutos. Sigue usando diario NVS acotado, no añade un nuevo diario en LittleFS ni borra claves. La política conservadora antigua permanece para otros backends. Si falla el contador tras guardar el diario se mantiene pendiente.
- Punto de control `lfs_gen` en NVS: una escritura verificada de una generación LittleFS nueva, sólo con arranque saludable, sin OTA y con margen. Si se detecta perfil anterior al conocido, se avisa y no se migra automáticamente la vieja copia NVS. No se baja el marcador. Si el marcador aún no existe, no puede garantizar detección de regresiones históricas. No impide la exportación/importación ni altera automáticamente las curvas; exige revisión del propietario. El marcador no es copia del perfil.
- Uptime y evidencia de eventos de 64 bits mediante reloj monotónico ESP; los temporizadores cortos de BLE conservan la aritmética modular previa. Reparación de epoch de arranque usa segundos monotónicos. Aumenta longitud de etiquetas del ring RAM sin aumentar su capacidad.
- Por solicitud del propietario se retira del candidato el servicio de vigilancia (rutas, buffer y UI, código Python de Mac/Linux, Telegram/MQTT, pruebas y paquetes del servicio). Se mantiene copia USB independiente, sin depender de monitor/. No se desinstala nada en el equipo ni se borran ramas históricas ni claves NVS.

## Invariantes

Mapa flash, perfiles de referencia, motor RGB, codificación BLE, transporte Bluedroid, prioridades OFF, mantenimiento, política Wi-Fi/OTA, PWA y curvas persistentes se conservan. La prueba de hashes antigua sigue verificando los rangos completos, revirtiendo en memoria sólo dos cambios concretos del flujo CH para comparar con los hashes originales. No se redistribuye BIN de otra versión como 3.13.

## Pruebas y límites

Run `python tools/check_b89.py`, `python tools/check_littlefs_migration_lab.py`, CI navegador y compilación ESP32-S3 C124. Se incluyen tests de código de producción y escenarios sintéticos de 49 días, pérdida CH, pausa, OFF, espacio NVS, fallo de contador, punto de control y rollback gen22→9. Se conserva la verificación de byte a byte de perfiles, simulador de cortes LittleFS y pruebas OTA anteriores.

Pendiente de validar en ATOM real: antes de instalar, exportar curvas activas y disponer del BIN 3.12/copia USB; observar arranque, checksum c1960ba7 y gen22 intactos, OFF nocturno y amanecer, 24 h, desconexión CH supervisada, diario y memoria. Confirmar ópticamente ambas luces. No inducir corrupción ni cortes durante uso normal. La compilación, tests y GATT no son certificación óptica.

Siguiente fase separada: reloj confiable y autonomía tras arranque sin router, identificadores para retorno personalizado 0x03 y callback tardío, persistencia LittleFS de históricos ampliados si sigue siendo necesaria, consumo de RAM de exportación. No cambiar esos mecanismos sin escenarios de fallo propios.
