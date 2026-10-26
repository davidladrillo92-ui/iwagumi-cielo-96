# RECOVERY3.7: guardado de curvas en LittleFS, sin formato

## Problema y resultado

Los informes82/83 confirman físicamente que la partición existente llamada
spiffs contiene LittleFS1.14.1: monta en lectura, tiene1.572.864bytes totales y
303.104 usados. El intento con SPIFFS falla y por eso las curvas continuaban en
NVS20KiB. La copia USB actual de8MiB fue verificada con dos lecturas idénticas y
cifrado/descifrado local en Sierra10.12.6/Python3.9.13/esptool4.8.1.

La candidata3.7 incorpora el backend LITTLEFS_AB. Al arrancar, migra el perfil
NVS validado, conservando todos sus788bytes y su generación9. No importa otra
curva, no recalcula colores y no modifica ninguno de los dos slots NVS. Los
guardados posteriores de cambios reales van a LittleFS; guardar un perfil
idéntico continúa devolviendo unchanged sin incrementar generación ni escribir.

La versión conserva los hashes fijados del motor, BLE, recuperación, apagado,
mantenimiento, UI, particiones y vigilante Sierra. Cambia almacenamiento,
diagnóstico de almacenamiento, versión y pruebas. No actualiza framework ni
transportes. No contiene formato, reparación, grow, borrado de particiones,
limpieza de archivos desconocidos ni NVS clear. La compatibilidad SPIFFS previa
se conserva para un dispositivo que realmente tenga ese formato.

## Montaje y autoridad de las copias

CurveStore prueba la compatibilidad existente y exige que la sonda RO termine
READ_ONLY_VERIFIED_AND_CLOSED antes de registrar LittleFS escribible en
/cielo-lfs. read_only=false solo en este segundo registro;
format_if_mount_failed=false y grow_on_mount=false siempre. Un montaje ajeno
se conserva; error/retención de montaje RO impide el registro RW.

Los snapshots nuevos son /cielo_lfs_a.bin y /cielo_lfs_b.bin. Solo se aceptan
con tamaño exacto, magic, esquema, puntos y hash válidos. En cada arranque se
elige la generación válida más reciente entre NVS y FS. Una generación igual
con bytes diferentes o una distancia uint32 ambigua bloquea guardados; no
se elige silenciosamente una copia para sustituir otra. Un slot existente
ilegible tampoco se trata como vacío. Se mantiene la curva legible disponible.

Si NVS tiene una versión más reciente tras usar un firmware anterior, se copia
esa versión a FS con la misma generación, conservando el snapshot FS previo.
Si FS ya contiene la misma copia, se carga sin reescribirla. Defaults en RAM
sin perfil persistente no se migran automáticamente.

Detectar firmas LittleFS/error de su lectura o intento de montaje impide el
fallback escribible a NVS. Así un fallo del nuevo backend no empieza una
segunda cadena de generaciones en NVS ni vuelve a presionarla. NVS se utiliza
para lectura de respaldo. Los diarios históricos y flags operativos heredados
siguen su política existente: no se afirma cero escrituras NVS del firmware.

## Publicación y recuperación de interrupciones

El mismo Publisher de include/cielo_littlefs_transaction.h se usa en firmware
y en el ensayo con LittleFS real. IO POSIX es un adaptador de open/write/fsync/
close/stat/read/rename sobre el VFS del SDK.

1. Validar candidato y ambos slots, orden de generaciones y copia activa.
2. Exigir32KiB de margen de trabajo. Es una cota conservadora, no una promesa
   de asignación: los errores reales de escritura/sincronización se propagan.
3. Buscar un nombre ausente entre /cielo_lfs_stage_00.bin y63. Nunca abrir,
   truncar ni borrar un temporal existente. Crear el elegido con O_EXCL.
4. Escribir788bytes, fsync y close comprobados. Releer y comparar exactamente.
5. Releer la copia activa justo antes de publicar; si cambió o no se lee, parar.
6. rename reemplaza solo el destino inactivo. No hay unlink previo ni ventana
   sin perfil activo. Releer el destino antes de activar RAM y comunicar éxito.

Un corte puede dejar un temporal parcial. Al reiniciar se ignora como perfil
activo y el reintento usa otro nombre ausente: **se recupera sin borrarlo**.
Incluso un temporal válido preexistente se conserva. El primer modelo de PR59
dejaba63/105 reintentos bloqueados por exigir reutilizar un solo nombre; este
protocolo supera ese límite sin asumir que el archivo incompleto sea basura.

El espacio y los nombres están acotados.64 nombres ocupados o margen insuficiente
bloquean guardados sin borrar nada; los perfiles activos siguen disponibles.
No se promete recuperación infinita bajo cortes repetidos ni limpieza automática.
Una futura retirada de temporales requerirá auditoría de sus metadatos/propiedad.
Un error durante rename o verificación final se marca incierto: un reintento
no puede saltarse la verificación por considerar idénticos los puntos en RAM.

## Pruebas y límites

- Suite host completa: incluye todas las pruebas heredadas de motor/BLE/UI,
  hashes de alcance congelados, API real de commit y fallback NVS.
- Publisher de producción con core LittleFS oficial fijado por el SDK1.14.1:
 132 cortes, antes/después/mitad de callbacks de programación/borrado y fronteras
 de sync. Reinicio desde bytes persistidos; perfil activo, NVS simulado y archivo
 ajeno conservados;132 reintentos recuperados,0 bloqueados.
- 1000 guardados idénticos sin mutaciones prog/erase/sync en el ensayo nativo.
- Temporales parciales, válidos y distintos conservados;64 nombres ocupados
  bloquean sin mutación. Falta real de espacio en fixture, hash inválido, slots
  ilegibles, conflicto de generación, wrap, flash vacía y lectura fallida.
- Adaptador POSIX real en directorio temporal: O_EXCL, write/fsync/close,
  lectura, sustitución del slot inactivo con rename, no-op y errores de espacio.
  Registro SDK simulado exige RO previo, no-format/no-grow y montaje propio.
- loadProfile real extraído del firmware: migración gen9/38PV/31CH exacta,
  selección de copia más reciente, reboot sin reescritura, conflictos y fallo de
  migración con NVS intacto. API saveProfile real impide fallback NVS cuando FS
  lo prohíbe. No se compila un segundo modelo del protocolo en sustitución.

Los contadores representan llamadas de archivo/API, no ciclos físicos de flash.
La simulación de cortes parciales cubre media operación, no todos los brownouts.
No demuestra durabilidad del chip, latencias del VFS/FS en el ATOM ni estabilidad
prolongada BLE. El adaptador VFS final requiere compilación con SDK real y prueba
física. La copia cifrada actual sigue privada en el iMac: estos ensayos no han
abierto sus contenidos. La primera instalación3.7 está pendiente de validación.

## Informe Maestro y primera prueba

curve_storage.backend y loaded_backend deben indicar LITTLEFS_AB tras una
migración verificada. fs_available=true, sin formato ni cambio de tabla. La
sonda RO queda registrada dentro de littlefs_readonly_probe; su propio campo
curve_writes_enabled=false describe esa sonda, no el backend RW posterior.

littlefs_write_backend incluye montaje/error, migración intentada/verificada,
bloqueo, errno, temporal elegido, nombres conservados, límite64, margen32768,
intentos/verificaciones, bytes de archivo, fsync, rename y publicación incierta.
Los flags de migración corresponden a este arranque: al cargar una copia FS ya
existente puede no haber intento nuevo de migración. Los slots NVS deben seguir
A9/B8 mientras no se vuelva a usar un firmware que escriba esas claves.

Después de instalar solo el BIN de aplicación por la web local, revisar:

1. Informe inicial: versión3.7, gen9/38PV/31CH, backend LITTLEFS_AB y migración
   verificada; ningún borrado/formato; copia A9/B8 válida en NVS.
2. A los5min: validación del arranque/OTA y comunicación de ambas luces. GATT
   sigue sin medir luz física. No mezclar una prueba de mantenimiento.
3. Guardar sin cambios: misma generación, unchanged=true y0 bytes nuevos.
4. Para verificar un cambio real sin alterar el aspecto del acuario, preparar
   un punto redundante sobre el tramo completamente apagado, revisar/exportar
   antes la curva y comprobar gen10/slot inactivo/reinicio. Esta edición no se
   realiza automáticamente ni se declara probada con el informe de arranque.

La vuelta a3.6.2 carga su respaldo NVS9, no las posteriores ediciones FS. Los
snapshots FS permanecen intactos y una nueva3.7 puede recuperarlos. Exportar
el perfil actual antes de retroceder si se necesita conservar también la última
programación en una versión antigua. No restaurar la copia8MiB por OTA.

Fuentes: esp_littlefs v1.14.1 src/esp_littlefs.c (O_EXCL, fsync y lfs_rename),
core fijado tests/vendor/littlefs/SOURCE.json y evidencia física informes82/83.
