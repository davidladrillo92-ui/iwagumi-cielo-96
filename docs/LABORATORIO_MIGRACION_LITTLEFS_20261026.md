# Preparación de migración: laboratorio sintético, 26/10/2026

## Resultado y alcance

Se añade un banco de pruebas de publicación de curvas sobre LittleFS real en
una memoria flash **sintética en RAM**. Es preparación anticipada del protocolo,
no migración de la copia histórica ni del ATOM actual. Ningún archivo anterior
de la fuente3.6.2 cambia:214 archivos comparados byte a byte con el ZIP de Actions
40405794238. No cambia versión/BIN, motor, BLE, UI, mantenimiento, particiones,
vigilante Sierra ni herramienta de copia. La candidata3.6.2 sigue solo lectura.

Pendientes: COMPROBAR en el iMac, copia actual cifrada, resultado físico de la
sonda3.6.2 y posterior ensayo con esa copia. No se declara solucionado todavía
el almacenamiento del dispositivo ni se activa escritura LittleFS.

## Prueba reproducible

Ejecutar en el checkout, sin argumentos ni dependencias Python externas:

```sh
python tools/check_littlefs_migration_lab.py
```

Requiere gcc/g++ host. Es un ensayo de desarrollo Linux, no un nuevo programa que
haya que instalar en el iMac. El lanzador no admite imágenes ni puertos USB, no
usa red, no abre copias privadas y solo compila en un directorio temporal.

Se incluyen sin modificaciones lfs.c, lfs.h, lfs_util.c, lfs_util.h y licencia
BSD3 de littlefs-project/littlefs, commit
f53a0cc961a8acac85f868b431d2f3e58e447ba3: el gitlink src/littlefs declarado por
esp_littlefs v1.14.1. SOURCE.json fija y comprueba sus hashes Git. No se reemplaza
el componente enlazado en el firmware. Tampoco se supone equivalencia entre el
ensayo nativo y las opciones de compilación/VFS del SDK instalado.

La fuente usa LFS_NO_MALLOC, cachés de tamaño fijo, sectores4096,384 bloques,
read/prog16 y disk format2.1. El único formateo es lfs_format sobre un vector
recién creado, para fabricar el ejemplo sintético. No existe acceso a flash
física ni una ruta que permita formatear una copia aportada por el usuario.

La curva se genera desde la fixture V5 existente:gen9,38PV/31CH,788bytes,
validación del mismo ProfileImage y hash del firmware. Un archivo ajeno
sintético de282079bytes sirve de testigo de conservación; su nombre y contenido
no proceden del respaldo privado. Otro vector de20KiB modela NVS fuera del
alcance de todos los callbacks FS; se compara completo en los ensayos. No es un
emulador de NVS ni valida aún el selector de perfiles del arranque real.

## Protocolo experimental

1. Validar perfil, tamaños, checksums y ambas copias A/B. Cualquier copia
   ilegible bloquea el intento; no se trata como hueco libre.
2. Rechazar generaciones antiguas, media vuelta ambigua de uint32, o igual
   generación con bytes diferentes. Igual generación y bytes iguales devuelve
   Unchanged sin escribir. La primera copia desde NVS conserva generación9.
3. Comprobar margen experimental de8 bloques (32KiB); no es una garantía de
   éxito ni una medida del espacio NVS. Un fallo posterior también se propaga.
4. Crear temporal solo si falta. Si ya existe, solo reutilizarlo cuando es
   válido e idéntico al candidato. Temporal parcial, desconocido o diferente:
   conservar y bloquear, para revisión posterior.
5. Sincronizar/cerrar, releer y comparar todos los bytes del temporal. Releer
   además el perfil activo justo antes de publicar.
6. Renombrar sobre el destino inactivo mediante LittleFS, sin quitar antes el
   perfil activo; releer el destino y verificar de nuevo. NVS no se borra ni se
   actualiza con un marcador. No se llama al selector de iluminación.

Este código vive solo en tests/littlefs_lab; no es un backend ESP32 habilitado.
No sustituye ni modifica el Store SPIFFS existente.

## Evidencia local

Cuatro escenarios: primera copia gen9 desde NVS simulado, actualización gen10
con dos slots válidos, actualización con un solo slot y publicación de temporal
ya validado. Respectivamente11,11,11 y2 fronteras de mutación. Para cada una se
simula corte antes, después y a mitad de programación/borrado (sync usa frontera).
Se abandona el lfs_t interrumpido y se monta otro con solo los bytes persistidos.
Las excepciones se propagan también por los frames C compilados con -fexceptions;
no se continúa artificialmente la operación tras el corte.

- 105 cortes: montaje posterior y perfil anterior o nuevo íntegro. Cuando ya
 había un perfil activo, se comprueba además que sus bytes sobreviven intactos.
- Archivo ajeno y vector NVS conservados en todos los casos.
- 1000 guardados idénticos:0 callbacks prog/erase/sync y bytes FS iguales.
- Rechazo sin mutación ante conflicto de generación, ambigüedad, perfil inválido,
 slot ilegible, temporal desconocido/diferente y error de lectura.
- Caso de espacio agotado por escrituras reales del relleno sintético: se bloquea
 nuevo guardado con el margen de trabajo y se recupera el perfil anterior.
- Flash vacía o lectura fallida: fallo de montaje,0 escrituras, sin formato.
- Cruce uint32 de generación4294967295 a0 verificado dentro del rango permitido.

**63 de105 cortes dejan el reintento bloqueado para revisión**. No es recuperación
automática completa: la política deliberadamente conserva temporales incompletos.
El perfil activo/NVS sigue disponible. Antes de una integración escribible habrá
que diseñar la recuperación explícita de temporales con propiedad demostrada,
sin eliminar un archivo preexistente por suponer que es basura.

Los contadores son callbacks de este dispositivo sintético, no medidas de
desgaste real del ATOM ni de páginas NVS. Los cortes parciales representan solo
un patrón de media operación; no cubren todos los brownouts, cachés de hardware,
fallos de chip o condiciones de concurrencia/tiempo del firmware.

El workflow independiente cielo-littlefs-lab ejecuta el ensayo con Python3.9 y
3.11 y conserva el resultado como artefacto. No genera ni distribuye otro BIN.

## Siguiente paso físico

Primero ejecutar COMPROBAR.command del paquete Sierra ya entregado y revisar su
JSON. Después hacer la copia actual cuando se pueda interrumpir el control por
USB. Una vez conservada la copia, probar la candidata3.6.2 y revisar gen9/38/31,
resultado LittleFS, heap y BLE en el Informe Maestro. Solo entonces ensayar una
migración sobre un duplicado privado de esa copia e integrar, si procede, un
backend escribible con recuperación de temporales y fallback NVS verificables.

Fuentes primarias del ensayo:
- https://github.com/joltwallet/esp_littlefs/tree/v1.14.1/src/littlefs
- https://github.com/littlefs-project/littlefs/tree/f53a0cc961a8acac85f868b431d2f3e58e447ba3
- tests/vendor/littlefs/SOURCE.json y LICENSE.md
