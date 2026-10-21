# Guía de copia USB, inspección y actualización RC3 → RC5 desde iPhone

Destino: `11.1.0-RC5-B1-B9-RECOVERY`, ATOMS3 Lite C124 ESP32-S3 8 MiB.
Usar exclusivamente el paquete del commit final con Actions SUCCESS indicado en
la PR #34. No el antiguo paquete MONITOR ni un ZIP/BIN de RC5 anterior.

## 1. Preparar una ventana supervisada

No hacer el estreno inmediatamente antes de salir dos días. Dejar tiempo para
observar una noche completa y recuperar por USB. Proteger Pez Verde mediante
alimentación independiente comprobada; no desconectar filtro ni calentador.
Cerrar My Chihiros y otras apps de control BLE durante los ensayos de CIELO.

Guardar el Informe Maestro actual y exportar el perfil real JSON desde RC3.
Los informes 41/42 indican PV34/CH31 generación 8; comprobar la exportación, no
volver a importar otra V5 por rutina. Guardar localmente clave OTA, acceso Wi-Fi
 y clave del AP. No pegarlas en GitHub, informes compartidos o este chat.

## 2. Comprobar el ordenador para el backup

La copia completa se realiza por USB desde un ordenador, no desde la web del iPhone.
En el iMac, abrir Terminal y obtener primero:

```sh
sw_vers
python3 --version
python3 -m esptool version
ls /dev/cu.*
```

Estos comandos no escriben flash. En macOS Sierra no damos por supuesto que
Python moderno y cryptography funcionen. Si faltan o fallan por compatibilidad,
utilizar un ordenador compatible; no borrar el ATOM ni actualizar el Mac para
forzar la operación. El puerto USB se identifica comparando la lista antes/después
de conectarlo con cable de datos. Conectar/reiniciar el ATOM puede afectar las luces.

En un equipo compatible, con Python 3.11 recomendado, extraer el paquete descargado
y abrir Terminal en esa carpeta (contiene `tools/`, `docs/` y el BIN):

```sh
python3 -m venv .venv
source .venv/bin/activate
python -m pip install esptool==4.8.1 cryptography==46.0.4
python -m esptool version
```

## 3. Copia completa ANTES de cambiar firmware

Crear una carpeta privada en disco cifrado, fuera de Git. Sustituir literalmente
los marcadores por el puerto y rutas observados; no ejecutar con los ejemplos:

```sh
python tools/cielo_flash.py backup --port PUERTO_OBSERVADO \
  --output /RUTA_PRIVADA/CIELO_RC3_8MiB.cieloenc --lights-safe
```

`--lights-safe` reconoce que ya has protegido las luces: no lo hace el script.
Pide una contraseña nueva de al menos 12 caracteres dos veces; no la muestra ni
la envía. Identifica el dispositivo, lee los 8.388.608 bytes dos veces sin arrancar
la aplicación entre lecturas, exige igualdad exacta, valida tabla y cifra el respaldo.
A 115.200 baudios puede tardar varios minutos. No desconectar mientras trabaja.

Si aparece un error, parar y conservar RC3. No se utiliza `erase_flash` ni `--force`.
Guardar una segunda copia del `.cieloenc` en otro soporte privado y la contraseña
por separado. No subir el dump completo, ni siquiera cifrado, al repositorio público.

## 4. Inspeccionar antes de decidir ninguna limpieza

```sh
python tools/cielo_flash.py inspect /RUTA_PRIVADA/CIELO_RC3_8MiB.cieloenc \
  > /RUTA_PRIVADA/INSPECCION_RC3.json
```

Se descifra y comprueba en memoria. El JSON contiene tamaño, SHA256, tabla real,
particiones totalmente `FF` y, para NVS reconocida, estados de páginas y contadores
de slots. No muestra claves ni valores. Puedes compartir **solo INSPECCION_RC3.json**
para revisar qué hay, si hay estados extraños y qué requeriría investigación.
No confundir slots borrados con archivos inútiles; no se infiere que un filesystem
contenga cero archivos solo porque no se haya montado. Listado SPIFFS opcional en
[USB_RECUPERACION.md](USB_RECUPERACION.md), siempre offline y con geometría comprobada.

No se ha hecho todavía esta lectura física. La ocupación real no puede conocerse
solo con los informes 41/42.

## 5. Qué significa instalar «limpia» en este caso

La opción elegida es cargar una aplicación compilada desde las fuentes verificadas
mediante la partición OTA inactiva. Conserva NVS, perfil V5, Wi-Fi, claves y la app
anterior para rollback. El BIN solo contiene aplicación; NO escribirlo a `0x0`.
No es un borrado total de fábrica y no elimina configuraciones ajenas que puedan
existir. Un borrado total destruiría el respaldo interno y exigiría reprovisionar
todo; los informes no aportan una razón para hacerlo. La limpieza adicional solo
se decide después de revisar la inspección, con backup íntegro disponible.

## 6. Primer salto desde RC3 usando el iPhone

**Distinción verificada en el código RC3:** su actualizador recibe un `.bin`, clave
local OTA, permiso físico y SHA256; no tiene selector de firma ECDSA. Esa firma se
exige desde RC4/RC5. Por eso la primera migración desde RC3 utiliza su mecanismo
normal. El nuevo BIN no debe anunciarse como firmado.

1. Descargar el paquete exacto de Actions indicado en la PR #34 y descomprimirlo
   en Archivos del iPhone. Conservar BIN, `SHA256.txt` y `BUILD_MANIFEST.json` juntos.
2. Comprobar que el manifiesto corresponde al commit final y a RECOVERY. Antes de
   pasarlo al iPhone, se pueden comprobar las huellas en el ordenador:

   ```sh
   shasum -a 256 CIELO_11_1_CANDIDATO_SOLO_APLICACION.bin
   ```

   Comparar los 64 caracteres con `application_sha256` del manifiesto y la PR.
   La huella prueba integridad frente a esa referencia; no sustituye una firma.
3. Reiniciar normalmente el ATOM después del backup, volver a la misma Wi-Fi y
   comprobar versión RC3, hora, perfil y BLE activo. Esperar al menos tres minutos.
   Si vuelve a bloquearse, no insistir con OTA: proteger luces y revisar por USB.
4. En Safari abrir la dirección local que ya usas para RC3. Usar su panel de
   actualización/servicio y la clave OTA que ya tienes; no crear otra si está emparejada.
5. Seleccionar **CIELO_11_1_CANDIDATO_SOLO_APLICACION.bin**, no ZIP, bootloader,
   particiones ni el respaldo de 8 MiB.
6. Mantener pulsado el botón del ATOM unos **5 segundos** y soltar dentro del rango
   de **3–7 segundos**. Eso abre el permiso físico de 90 s. No mantenerlo más tiempo
   ni hacerlo si no puedes supervisar las luces. Renovarlo si caduca.
7. Iniciar la instalación. RC3 comprueba salud BLE y prepara las pantallas antes de
   cargar. Esa preparación no garantiza apagado óptico de Pez Verde: la protección
   eléctrica independiente permanece. Si la preparación falla, parar, no forzar.
8. Mantener Safari en primer plano y el iPhone conectado a esa Wi-Fi. No cambiar
   apps, bloquear pantalla ni cortar corriente hasta finalizar y comprobar el reinicio.

## 7. Primer acceso a RC5 y verificación

RC5 sirve la aplicación por HTTPS y crea un certificado propio si aún no existe.
Durante este primer arranque, mirar por USB serie a 115200 la huella SHA256 que
imprime. No compartir la salida completa: también puede contener la clave del AP.
El lector serie debe estar cerrado mientras se usa esptool para backup/restauración.

Abrir `https://iwagumi-cielo96.local` si resuelve en la LAN, o la dirección local
real del ATOM por HTTPS. No cambiarlo por `cielo.local`.
Safari puede advertir por el certificado autofirmado: cotejar la identidad obtenida
por USB. Si el iPhone no permite establecer esa confianza, parar y resolver ese
paso antes de introducir la clave; no se ofrece una escritura alternativa por HTTP.
La compatibilidad y aceptación exactas de Safari siguen pendientes de ensayo real.

Abrir sesión con la clave OTA existente. RC5 conserva su huella antigua para rollback
y migra la comprobación a PBKDF2 cuando autenticas correctamente.

Comprobar y guardar un Informe Maestro nuevo:

- Versión RECOVERY exacta, perfil PV34/CH31 y exportación igual a la guardada.
- Reloj correcto, Wi-Fi estable y worker BLE con heartbeat reciente.
- Comunicaciones nuevas con ambas lámparas; mirar también el resultado físico.
- Transición Chihiros 2/2/2 → 1/1/2 a las 23:00 y POWER OFF físico Pez Verde
  en el punto de apagado configurado. No alterar RGB para realizar la migración.
- Retorno 0x03 a My Chihiros probado visualmente.
- Diagnóstico `system.ble_recovery` presente; un arranque normal por OTA no debe
  aparecer como rescate automático por bloqueo.
- Salud posarranque OTA y estabilidad 24–72 h, incluidos cambios de franja y noche.

La prueba del nuevo supervisor con bloqueo provocado se hace en banco bajo control,
no añadiendo un botón de bloqueo a la aplicación del acuario. Un resultado GATT
no sustituye mirar las pantallas.

## 8. Actualizaciones posteriores y recuperación

**Una vez en RC5**, las próximas cargas exigirán HTTPS y firma ECDSA con la clave
privada original, además de los controles locales. Esa privada no está en el ATOM
ni en el repositorio. Debe estar localizada y respaldada antes de depender de OTA.
No generar otra clave ni sustituir la pública integrada. El firmador ahora rechaza
una privada que no corresponda a esa identidad.

```sh
python tools/sign_firmware.py CIELO_11_1_CANDIDATO_SOLO_APLICACION.bin \
  --private-key /RUTA_PRIVADA/CLAVE_ORIGINAL.pem
```

Esto se ejecuta localmente solo si se dispone de la original; no pedir ni subir la
privada al chat. El `.sig` generado corresponde exclusivamente a ese BIN.

Si RC5 no valida salud al arrancar, existe rollback programado a la partición anterior
(10 min sin completar salud o tres arranques fallidos). No prueba por sí solo luz
correcta y requiere ensayo físico. Conservar alimentación y protección externa.
Si hace falta restauración USB completa, obtener primero el plan sin escribir:

```sh
python tools/cielo_flash.py restore /RUTA_PRIVADA/CIELO_RC3_8MiB.cieloenc \
  --port PUERTO_OBSERVADO
```

La escritura real exige además `--execute --lights-safe --confirm-sha256 HASH_EXACTO`;
seguir [USB_RECUPERACION.md](USB_RECUPERACION.md), misma placa y posterior verificación
completa. No realizarla como prueba rutinaria ni sobrescribir una copia anterior.
