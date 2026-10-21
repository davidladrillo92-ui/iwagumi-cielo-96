# B9 · Respaldo e inspección USB privados

**Pendiente de ejecutar en el ATOMS3 real.** Los ZIP de fuentes y BIN de aplicación
no son copias de los 8 MiB. Conectar por USB puede reiniciar el ATOM e interrumpir
Bluetooth: estabilizar antes las luces con control independiente. No cortar filtro/calefacción.

## Entorno

`tools/cielo_flash.py` usa Python 3.8+ (preferible 3.11), `cryptography` y esptool 4/5.
La imagen candidata se compila en CI; esto no requiere que el iMac compile firmware.
En macOS Sierra hay que comprobar qué Python/USB es realmente compatible antes de seguir;
no se presupone que paquetes modernos se instalen allí. Se puede hacer la copia en otro
ordenador. No actualizar sistema ni borrar flash como solución de prueba.

Preparar en un equipo compatible:

```sh
python3 -m pip install esptool==4.8.1 cryptography==46.0.4
python3 -m esptool version
ls /dev/cu.usb*
```

El puerto debe ser el observado. Los comandos siguientes utilizan `PUERTO_OBSERVADO`
y rutas de ejemplo, no un puerto supuesto.

## Copia sin escribir flash

```sh
python3 tools/cielo_flash.py backup --port PUERTO_OBSERVADO \
  --output /ruta/privada/CIELO_RC3_8MiB.cieloenc --lights-safe
```

Se identifica ESP32-S3, se lee `0x0..0x800000` dos veces sin arrancar aplicación entre
ellas, se exige igualdad byte a byte y tamaño exacto. Se valida tabla a 0x8000,
checksum MD5 cuando existe, límites, solapes y particiones obligatorias. Luego se
cifra con AES-256-GCM y contraseña derivada por Scrypt; se descifra en memoria y
verifica antes de guardar. El archivo contiene además SHA256 e identidad derivada
 del dispositivo para impedir restaurar accidentalmente otra placa.

El destino no puede existir. Archivo `0600`, directorio temporal privado, sin uso de
shell ni `erase_flash`, `--force` o cambios de particiones. Las lecturas temporales
se eliminan al terminar; eso no garantiza borrado físico del disco. Usar un disco
cifrado y conservar contraseña fuera del archivo, con una segunda copia privada.
**Nunca subir el dump, el archivo cifrado, claves o NVS al repositorio público.**

Un error cancela la operación. Si el segundo intento falla y el ATOM permanece en
bootloader, recuperar su arranque normal solo después de comprobar la contingencia
de iluminación; no improvisar una escritura.

## Inspección y archivos

```sh
python3 tools/cielo_flash.py inspect /ruta/privada/CIELO_RC3_8MiB.cieloenc
```

Muestra metadatos, SHA256 y particiones; no valores NVS, Wi-Fi o claves.
Para listar archivos hay que usar la imagen offline y una utilidad SPIFFS compatible:

```sh
python3 tools/cielo_flash.py inspect /ruta/privada/CIELO_RC3_8MiB.cieloenc \
  --extract-spiffs /ruta/privada/spiffs.bin
python3 tools/cielo_spiffs.py /ruta/privada/spiffs.bin \
  --mkspiffs /ruta/al/mkspiffs --block BLOQUE_REAL --page PAGINA_REAL
```

No adivinar la geometría ni montar/formatear el filesystem del ATOM para explorar.
Si está vacío, usa otro formato o la herramienta no es compatible, registrar
**no inspeccionado**, no «sin archivos». Listados y extraídos permanecen privados.
La consulta **Ajustes → Mantenimiento** del firmware candidato informa la tabla
real, NVS usada/libre, heap y partición activa sin montar el filesystem.

## Recuperación (destructiva, siempre explícita)

Primero obtener el plan, que no contacta USB:

```sh
python3 tools/cielo_flash.py restore /ruta/privada/CIELO_RC3_8MiB.cieloenc \
  --port PUERTO_OBSERVADO
```

Revisar identidad, tabla y SHA256. Solo cuando se haya decidido sobrescribir la flash
completa del **mismo ATOMS3** y las luces estén protegidas:

```sh
python3 tools/cielo_flash.py restore /ruta/privada/CIELO_RC3_8MiB.cieloenc \
  --port PUERTO_OBSERVADO --execute --lights-safe --confirm-sha256 HASH_EXACTO_DEL_PLAN
```

No modifica parámetros del encabezado, no fuerza acceso y no desactiva protecciones.
Tras escribir exige lectura completa igual al dump antes de dar recuperación por verificada.
Si hay Secure Boot/Flash Encryption o esptool rechaza la operación, detenerse y revisar
el estado real; el script no permite saltarse protecciones. Un `pack` de dump externo
sin identidad no puede restaurarse con `--execute` mediante este script.

La comparación confirma bytes, no luces: comprobar perfil, fecha, sesión, programación,
POWER OFF físico y retorno 0x03. Una restauración también recupera claves/configuración
antiguas; actualizar los clientes del observador con identidad verificada si corresponde.
