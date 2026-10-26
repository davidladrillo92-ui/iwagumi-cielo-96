# Preparación LittleFS de solo lectura y copia actual en Sierra

Base fuente RECOVERY3.6.1 d441fe1c5d0b217b4dbbf0c3cebb8d6b759ced61.
Se conserva la iluminación, BLE, apagado, mantenimiento, UI, curvas y tabla
mediante los hashes de alcance ya fijados. Sin actualización del framework ni
del vigilante Sierra4.0.1. No hay instalación física desde las herramientas.

## Paso práctico: paquete de copia USB

CIELO_COPIA_USB_SIERRA_V1_0_0.zip contiene dos lanzadores Bash3.2:

- COMPROBAR.command: usa el localizador de Python3.9 ya utilizado por el
  vigilante. Comprueba esptool4/5 con su comando version (sin puerto), prueba
  AES-GCM/Scrypt y enumera puertos sin abrirlos. No instala dependencias, crea
  entornos, usa red ni cambia configuración del vigilante. Se puede ejecutar
  sin conectar aún el ATOM; en ese caso el puerto estará pendiente.
- HACER_COPIA.command: repite la comprobación y pide confirmación local antes
  de pausar/reiniciar el ATOM. Puerto único observado o elección de la lista,
  sin inventar un puerto. Contraseña nueva oculta, dos veces, mínimo12 caracteres.
  Si el entorno no está listo no se abre USB. No funciona de forma no interactiva.

Secuencia esptool: read_mac deja el ROM loader activo; primera lectura de8MiB
sin reset previo/posterior; segunda lectura sin reset previo y con reset final.
No se ejecuta la aplicación entre lecturas. Se exige igualdad exacta, tabla de
particiones válida, cifrado y descifrado de comprobación. MAC se sustituye por
SHA256 y las salidas USB privadas no se imprimen. Los únicos comandos permitidos
son read_mac/read_flash; el paquete no incluye comandos de escritura, borrado,
restauración ni OTA. El stub en RAM y el reinicio de lectura no son cambios de
firmware, pero interrumpen temporalmente el control de luces del ATOM.

Se guardan un .cieloenc compatible con el respaldo anterior y un JSON de
metadatos sin contraseñas/contenido NVS, en RESULTADOS_PRIVADOS (0700), archivos0600.
Lecturas en bruto solo en temporales privadas, eliminadas al terminar; no se
promete borrado seguro del disco. La contraseña no se guarda ni se envía. No hay
subida automática. Los reinicios normales pueden avanzar boot_id y activar avisos
del vigilante; eso no es un fallo de BLE inducido por un nuevo firmware.

El generador extrae por AST solamente las funciones de metadatos/cifrado del
tooling ya auditado. No distribuye su CLI de restauración ni datos privados.
Si falta esptool/cifrado en el iMac, el informe de comprobación permite preparar
una instalación compatible concreta posteriormente. No se intenta resolverlo
instalando versiones nuevas indiscriminadamente sobre Sierra.

## Candidata3.6.2: sonda LittleFS de solo lectura

Solo se ejecuta una vez por arranque si SPIFFS no está disponible; las consultas
posteriores leen el resultado RAM. Comprueba geometría exacta y flag de cifrado,
lee dos prefijos16B de bloques0/1 y exige al menos una firma littlefs. Un error
de lectura detiene la sonda. La firma no se presenta como validación suficiente.

Usa la API del componente LittleFS ya enlazado por Arduino2.0.17:

    read_only=true
    format_if_mount_failed=false
    dont_mount=false
    grow_on_mount=false

La estructura del SDK debe soportar explícitamente estos campos; si no, el build
falla. No se reemplaza BLE ni el framework para ocultar una incompatibilidad.
Ruta VFS /cielo-lfs-ro (dentro del límite15 caracteres). No abre archivos de
usuario ni expone nombres/contenido: monta, consulta total/usado y desmonta.
Un montaje previo ajeno se deja intacto. Si falla info siempre intenta desmontar.
Si falla desmontar, informa retained_readonly_mount=true; nunca cambia a escritura.

Registro VFS, info y desmontaje tienen sus códigos esp_err_t separados. El estado
READ_ONLY_VERIFIED_AND_CLOSED solo se emite si todo terminó y las estadísticas
son plausibles. Un error no activa formato, grow, check, reparación ni migración.
El Store de curvas sigue sin habilitar LittleFS como backend escribible.

Informe Maestro: curve_storage.littlefs_readonly_probe, con versión del driver,
firmas, códigos, total/usado, política y resultado. Si la sonda tiene éxito seguirá
apareciendo backend NVS_LEGACY: es deliberado, no una migración fallida.

## Validación

Sonda compilada con mock del SDK: flags estrictos, geometría, lectura inicial y
espejo, errores de registro/info/desmontaje, cifrado, montaje ajeno, estadísticas
imposibles y caché de una sola ejecución. Los hashes del motor original siguen
pasando. Suite completa previa, build S3 y Sierra Python3.9 en Actions.

Copia:6 pruebas reales de las funciones del paquete con USB simulado, secuencia
de reseteo para esptool4/5, rechazo de cualquier comando/rango distinto, lecturas
divergentes, recuperación al fallar, cifrado compatible, permisos, JSON sin secretos,
paquete limpio, Python3.9 y sintaxis shell. No se conectó hardware en estas pruebas.

La copia histórica19/10 había montado LittleFS2.1 en host con0 escrituras; no
demuestra la versión/configuración enlazada en el firmware ni la situación actual.
La sonda nueva debe confirmarlo físicamente después de conservar la copia actual.
El mínimo de heap y latencia de arranque deben revisarse en el siguiente informe.
No se declara validación física en iMac/Sierra ni ATOM antes de esa prueba.

## Orden de uso

1. Abrir COMPROBAR en el iMac y revisar el JSON de comprobación.
2. Con luces preparadas para la interrupción, conectar por USB y hacer la copia.
3. Conservar copia cifrada/contraseña; compartir solo el informe de metadatos.
4. Después valorar instalar la candidata solo lectura y revisar Informe Maestro:
   gen9/38PV/31CH, políticas RO y resultado LittleFS, memoria y BLE.
5. Solo tras verificar copia actual y lectura compatible se desarrollará la
   migración de snapshots en copias de prueba; NVS conservará su respaldo.

Fuentes primarias comprobadas: Arduino-ESP32 2.0.17
tools/sdk/esp32s3/include/esp_littlefs/include/esp_littlefs.h (componente1.14.1),
joltwallet/esp_littlefs v1.14.1/src/esp_littlefs.c (registro RO sin funciones VFS
de escritura, mount y grow separados), esptool v4.8.1/pyproject.toml (Python>=3.7).
La compatibilidad final de Sierra se comprobará con el entorno observado, sin
asumir que una dependencia moderna tiene wheels para10.12.
