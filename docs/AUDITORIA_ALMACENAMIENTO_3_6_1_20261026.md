# Auditoría SPIFFS/NVS — RECOVERY3.6.1

Alcance autorizado: almacenamiento. Base RC5 RECOVERY3.6, commit
df2e9801086e6a48a419dd4a1e5c8033df038bc4. Sin cambios en motor de iluminación,
trabajador BLE, cadencias, mantenimiento, retorno de las pantallas, curvas,
UI, tabla de particiones ni observador Sierra.

## Evidencia del informe81

26/10/2026 21:38 Europe/Madrid: RECOVERY3.6 instalada, boot132,4638s de
actividad. Generación9/38PV/31CH cargada desde NVS; curvesA9 coincide con RAM y
curvesB8 es válida. **La curva sobrevivió al reinicio desde boot131.**
Sin fallos de guardado en este arranque y sin nuevos intentos de guardado.
SPIFFS sigue UNMOUNTABLE_NONEMPTY_PRESERVED. NVS474 usadas/156 libres/630 totales,
8 namespaces. Journal no crítico omitido17 veces por reserva de espacio; el
temporizador3.6 ya evitó el crecimiento por cada paso del loop. Ambos enlaces
BLE activos,76PV y75CH aceptadas,0 errores o desconexiones inesperadas. El modo
de mantenimiento no se probó en ese informe (ticks0); no hay verificación óptica.

## Hallazgos

1. El antiguo erased() devolvía false tanto al encontrar un byte distinto deFF
   como al fallar esp_partition_read(). Por tanto, el estado NONEMPTY de los
   informes existentes **no prueba por sí solo** que la lectura hubiese encontrado
   contenido: también podía ocultar un fallo de lectura. No demuestra corrupción.
2. SPIFFS.begin() de Arduino-ESP32 2.0.17 devuelve solo bool. Un false puede
   corresponder a errores distintos del registro VFS/montaje. La versión anterior
   no guardaba ese esp_err_t; no puede reconstruirse a partir del informe.
3. Todavía existía una vía automática SPIFFS.begin(true) para una partición
   completamenteFF. Se elimina para cumplir la instrucción de ningún formateo,
   incluso vacío. Una partición vacía seguirá sin inicializar mientras rija esa
   restricción. La montada y válida continúa operando con sus snapshots.
4. El API de guardar incrementaba generación incluso cuando todos los puntos
   recibidos coincidían con el perfil activo. Esto cambia el blob y puede requerir
   una nueva escritura788B por una pulsación sin cambios. Ahora se evita.
5. No confundir solicitudes de API con desgaste: el propio SDK compara blobs y
   valores ya existentes y puede evitar escrituras físicas si son idénticos.
   El contador922303 del informe80 era de comprobaciones omitidas, no de flash.
   No se dispone de un contador físico de sectores escritos/borrados del ATOM.

Los156 libres estadísticos no garantizan una escritura de blob: espacio de GC,
distribución de páginas y copia antes de retirar la versión anterior importan.
El informe81 no contiene las páginas físicas. No se afirma fragmentación medida.

## Cambios de desarrollo

- Montaje únicamente sin formato. Distinguir ALL_FF, NON_FF_FOUND, READ_ERROR y
  NOT_READ; informar código exacto de lectura, bytes leídos, primer offset noFF,
  dirección/tamaño y flag cifrado. El primer noFF detiene la sonda; no es un
  inventario completo. No exponer bytes de contenido, claves ni credenciales.
- Rechazar una partición cifrada no compatible con el driver SPIFFS auditado.
  Si la tabla no coincide, no leer offsets desconocidos.
- mount_error_code_available=false declara que el bool de Arduino no ofrece el
  código del fallo de montaje. raw_read_error_code corresponde exclusivamente a
  esp_partition_read; no se presenta como error del montaje.
- Ninguna llamada nueva a format, check, reparación, GC manual, erase-all ni
  reparticionado. La protección de snapshots FS3.5.1 y fallback NVS3.5.2 permanece.
- Guardado sin cambios: validar formato, generación esperada, checksum y puntos;
  comparar puntos significativos. Si hay un perfil persistido y ningún resultado
  incierto previo, responder200/unchanged=true con la generación actual, sin
  llamar a saveProfile, rotar slots, escribir ni reiniciar las consignas de luz.
  Primer guardado de perfil RAM y resultados inciertos siguen la transacción
  normal. Un cambio real mantiene doble slot y verificación antes de activar RAM.
- Contadores RAM de llamadas nvs_set_blob de curvas y de blobs restart_v2 y
  diag_journal: intentos, aceptadas y bytes. No incluyen todas las claves ni
  describen sectores físicos. No cambian frecuencia/prioridad de los journals.
  Los campos previos del último intento real de escritura permanecen históricos
  cuando se omite un guardado idéntico; last_curve_request_unchanged lo distingue.

## Resolver la causa sin formatear

No se ha recuperado el almacenamiento del ATOM con este cambio. Se localizó y
analizó la copia privada completa del19/10/2026; no equivale a una copia del estado
actual. No variar geometrías ni ejecutar check sobre el ATOM por ensayo y error.

### Hallazgo confirmado en la copia histórica

La tabla validada sitúa la partición llamada spiffs en0x670000/0x180000. Su contenido
monta con el código oficial littlefs-project/littlefs v2.9.3, compilado con
LFS_READONLY, archivo de entrada abierto rb y callbacks de escritura/borrado
rechazados. Resultado: mount=0,stat=0,list=0,versión de disco0x00020001(LittleFS2.1),
bloques4096,384bloques,72usados,2archivos,282079bytes de archivos,0directorios,
488lecturas y0callbacks de escritura/borrado. SHA de entrada antes/después idéntica.
No se publican contenido, nombres de archivos, NVS ni credenciales del respaldo.

No había bloques enteramenteFF, pero LittleFS informa solo72bloques usados:
los bytes noFF y bloques no borrados incluyen datos obsoletos y no equivalen a
ocupación útil del filesystem. Esa copia no demuestra falta de capacidad.

Esto confirma LittleFS válido para el montaje/listado de esa copia. No demuestra
integridad de cada payload ni que la imagen actual sea idéntica. La firma literal
littlefs estaba a offsets8 y4104; el montaje independiente valida el indicio.
El nombre de partición spiffs NO determina su formato. Intentar montarla mediante
SPIFFS es incompatible con este contenido y explica el fallo de la copia; falta
confirmar el mismo estado en el dispositivo actual antes de migrar escrituras.

Se incluye el auditor C de solo lectura en tools/cielo_littlefs_readonly_audit.c.
Necesita lfs.c,lfs.h,lfs_util.c,lfs_util.h del tag oficialv2.9.3; se compila con
-DLFS_READONLY -DLFS_NO_DEBUG -DLFS_NO_WARN -DLFS_NO_ERROR. Solo enumera metadatos,
no extrae contenido ni contacta hardware. La entrada original se mantuvo intacta.

### Adaptación del controlador: siguiente fase

Arduino-ESP32 2.0.17 ya proporciona LittleFS, pero su begin(false) configura
read_only=false y grow_on_mount=true. Desactivar formato no es una promesa de
montaje sin escrituras. Por eso la inspección controlada debe registrar el VFS
LittleFS con read_only=true,format_if_mount_failed=false,grow_on_mount=false,
comprobando la compatibilidad de versión/geometría del componente enlazado.
No sustituir el framework ni actualizar BLE para este propósito.

Tras verificar copia reciente, archivos existentes y capacidad disponible:
probar snapshots de curvas en una copia desechable con el mismo controlador,
validar cortes/CRC/rename y diseñar la migración no destructiva conservando NVS.
Este PR no activa a ciegas escritura LittleFS en el ATOM y no promete que los
1.5MiB estén ya disponibles para las curvas.

Ruta verificable:

1. Obtener y conservar dos copias coincidentes de la flash original, en privado.
   La utilidad USB existente lee dos veces y cifra; requiere detener/reiniciar
   temporalmente el ATOM y preparar las luces. No se ejecutó desde esta auditoría.
2. Validar tabla, offsets, flags y checksum. Extraer spiffs de la copia, no del
   dispositivo en uso; conservar la imagen original inalterada.
3. Ejecutar tools/cielo_storage_audit.py sobre una copia local de la partición,
   o con --full-flash sobre8MiB. --compare exige una segunda copia idéntica.
   Muestra hashes, ocupación por bloques y bitmap NVS si procede; no monta,
   repara, escribe archivos de entrada ni contacta USB/red. Un texto littlefs
   se informa solo como indicio, nunca como identificación concluyente.
4. Comparar parámetros compilados reales con los de la imagen y la versión que
   la creó. Usar la herramienta de listado SPIFFS existente con geometría explícita
   solamente sobre copias locales. No certificar incompatibilidad/corrupción sin
   esa prueba. Probar cualquier reparación exclusivamente sobre copias desechables
   y comprobar archivos recuperados y hashes antes de diseñar una migración.

El binario de aplicación OTA por sí solo no permite inspeccionar ni recuperar los
datos de esa partición. La herramienta de copias funciona con Python3.9 y no obliga
a actualizar el vigilante de Sierra. No se ha borrado nada del ATOM.

## Validación

Hashes fijan los bloques originales del planificador, BLE, recuperación, programa
y mantenimiento, además del trabajador, interpolación, protocolo/cola, UI y tabla.
Se compararon con el snapshot base antes de publicar. Las pruebas impiden cambios
en esas partes. Pruebas nuevas: lectura fallida inicial/tardía, byte noFF al final,
vacío/cifrado/montado; cero formatos. Handlers reales de guardado: no-op repetido,
generación obsoleta, checksum erróneo, primer guardado RAM, resultado incierto y
edición real. Análisis de copias: entrada intacta, tamaño, dos copias y rechazo de
copias distintas. Suite previa NVS/SPIFFS/BLE/OTA/mantenimiento/Sierra conservada.

Resultado de compilación y hash candidato en el PR de entrega. Validación física
de3.6.1 y causa exacta del montaje pendientes. No presentar esta versión como
«SPIFFS reparada» ni recomendar instalarla como recuperación ya demostrada.

## Fuentes primarias consultadas

- Arduino-ESP32 2.0.17, libraries/SPIFFS/src/SPIFFS.cpp: begin devuelve bool y
  puede formatear solo si formatOnFail; aquí se permite únicamente false.
  https://github.com/espressif/arduino-esp32/blob/2.0.17/libraries/SPIFFS/src/SPIFFS.cpp
- ESP-IDF v4.4.7, components/spiffs/esp_spiffs.c: registro/montaje, errores,
  configuración de página y rechazo de particiones cifradas.
  https://github.com/espressif/esp-idf/blob/v4.4.7/components/spiffs/esp_spiffs.c
- ESP-IDF v4.4.7, nvs_storage.cpp, Storage::writeItem: comparación previa a
  escritura y publicación de la versión nueva antes de retirar la antigua.
  https://github.com/espressif/esp-idf/blob/v4.4.7/components/nvs_flash/src/nvs_storage.cpp


Fuente primaria adicional: Arduino-ESP32 2.0.17 libraries/LittleFS/src/LittleFS.cpp,
campos read_only y grow_on_mount observados; repositorio oficial LittleFSv2.9.3.
https://github.com/espressif/arduino-esp32/blob/2.0.17/libraries/LittleFS/src/LittleFS.cpp
https://github.com/littlefs-project/littlefs/tree/v2.9.3
