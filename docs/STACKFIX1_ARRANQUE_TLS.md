# RC5 STACKFIX1 · desbordamiento de pila durante arranque HTTPS

Candidata nueva: **11.1.0-RC5-B1-B9-STACKFIX1**. Instalada confirmada: RC3.
La candidata RECOVERY anterior (0805ed5, BIN SHA256
8032a9e81aad729991434e8ffcd3595a98a03c3da2b5285dfaf9de02e24d2db5)
**ha fallado físicamente en el primer arranque y no debe reinstalarse**.

## Evidencia física recibida el 22/10/2026

RC3 recibió y validó los 1.759.136 B. Se reinició y ejecutó RC5 RECOVERY en la
partición candidata. Después de CURVAS_OK e INICIO, antes de WEB_PRIMERO:

```
Guru Meditation Error: Core 1 panic'ed (Unhandled debug exception).
Debug exception reason: Stack canary watchpoint triggered (loopTask)
ELF file SHA256: 5f9db70edf08c9fd
```

Lo repitió en el segundo arranque. El tercero detectó el límite de intentos y
regresó a app0/RC3. Por tanto la transferencia, el comienzo de ejecución y el
retorno de tres arranques han sido observados; la imagen RECOVERY no superó
el inicio. No es evidencia de que la flash esté llena o la V5 esté dañada.

El informe 46 posterior conserva perfil PV34/CH31 generación 8 y muestra las
curvas pausadas. No acredita apagado físico de las pantallas.

El fallo de pila queda identificado por el canary. La fase HTTPS se localiza por
el orden de setup y los mensajes; no se anuncia una simbolización exacta del
backtrace original porque aquel paquete no conservaba su ELF. No se atribuye a
BLE: el fallo sucede antes de inicializarlo en esta nueva imagen.

## Corrección

Arduino-ESP32 2.0.17 crea loopTask con 8192 B por defecto. setup, provisión TLS y
posteriores operaciones criptográficas de CIELO se ejecutan en esa misma tarea.
Tener 12288 B asignados al servidor HTTPS no ampliaba loopTask: es otra tarea.

- Override oficial SET_LOOP_TASK_STACK_SIZE a 32768 B, comprobado como símbolo
  fuerte en el ELF final para que no quede activo el valor débil de Arduino.
- Buffers PEM de 1024+2048 B en una reserva temporal de heap, con fallo comprobado,
  limpieza RAII y borrado explícito del buffer privado al salir. No arrays de 3 KiB
  en la pila de provisión.
- Mediciones USB antes/después de HTTPS y campo system.loop_stack_min_free_bytes
  en Informe Maestro. ESP-IDF expresa esta marca en bytes.
- Informe del compilador -fstack-usage y STACK_REPORT.json. No calcula el peor
  caso dinámico completo de mbedTLS ni sustituye la medición real.
- Artefacto aparte con ELF y mapa para poder simbolizar futuras incidencias.

No se desactiva el canary ni se amplía el contador de arranques fallidos. Se
conservan las comprobaciones OTA y el retorno a RC3. Las referencias V5, protocolo,
motor, particiones y clave pública original permanecen fijados por hash.

## Repetición supervisada desde RC3

1. Conservar el respaldo USB ya obtenido. Mantener alimentación independiente
   segura de Pez Verde, sin afectar filtro ni calentador.
2. Descargar exclusivamente el nuevo paquete STACKFIX1 publicado en PR #34.
   Seleccionar **IWAGUMI_CIELO_RC5_STACKFIX1.bin**. El BIN genérico incluido es una
   copia idéntica; el nombre explícito evita mezclarlo con RECOVERY.
3. Mantener el monitor USB abierto, esperar 180 s de salud RC3 y usar el mecanismo
   de actualización RC3 con clave local y autorización física. Es una primera
   migración desde RC3; el nuevo BIN no se anuncia firmado ECDSA.
4. Verificar el banner STACKFIX1, STACK antes HTTPS, STACK después HTTPS,
   HTTPS_INICIADO, WEB_PRIMERO y BLE_PILOTO_PREPARADO. Ningún PANIC.
5. Abrir por HTTPS, comprobar certificado por USB, acceder con clave existente y
   descargar un Informe Maestro nuevo. La migración de credencial permanece.
6. Examinar margen de pila, heap, tiempo de actividad y perfil 34/31. Si el control
   estaba pausado antes de cargar, no darlo por reanudado automáticamente: decidir
   su activación después de comprobar hora, V5 y el estado físico de las pantallas.
7. Verificar POWER OFF real, retorno 0x03, primer inicio con certificado nuevo,
   reinicio con certificado conservado y estabilidad nocturna antes de uso sin
   supervisión. La corrección compilada aún necesita ensayo físico.

Si vuelve a fallar, conservar el log y detener nuevos intentos. No borrar NVS,
certificados o flash ni forzar la selección de la partición candidata.

Referencias de implementación: [Arduino.h](https://github.com/espressif/arduino-esp32/blob/2.0.17/cores/esp32/Arduino.h)
y [main.cpp de Arduino](https://github.com/espressif/arduino-esp32/blob/2.0.17/cores/esp32/main.cpp).
