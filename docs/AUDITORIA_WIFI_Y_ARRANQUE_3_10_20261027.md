# RECOVERY3.10 — Guardado Wi-Fi coherente y revisión de arranque

27/10/2026. Candidata basada exactamente en3.9, fuente b05a8d30048e6dd90fbee3020ec802ff4fbb3562 (PR62). Última evidencia física disponible: informe90, firmware3.8; capturas12:19 de la interfaz3.8.1. No se ha recibido todavía un Informe Maestro de3.9. No se ha accedido al ATOM, al iMac ni al bot Telegram desde este desarrollo.

## Qué se corrige

El código3.9 guardaba `ssid` y `wifi_pw` con dos `Preferences.putString` independientes y no comprobaba sus resultados. Después sustituía las variables de conexión y desconectaba aunque una escritura hubiera fallado. Eso permitía una configuración dividida o una confirmación incorrecta. Además, el SDK Arduino-ESP32 puede mantener su propia configuración persistente, aparte de Preferences.

La antigua acción Olvidar eliminaba dos claves. Si quedaban `homeSsid/homePass` en `iwagumi-rgb`, el siguiente arranque podía volver a migrarlas.3.10 guarda una desactivación explícita y llama al botón **Desactivar red guardada**. No promete borrar físicamente las credenciales antiguas.

No se atribuyen a este hallazgo los bloqueos BLE previos: son rutas distintas. Tampoco se afirma que el fallo NVS de curvas quede resuelto por este cambio; las curvas siguen usando el LittleFS A/B validado en3.7.

## Implementación

- Un registro de112bytes contiene SSID, contraseña, habilitación, versión, generación y checksum FNV. El checksum detecta alteraciones accidentales; no cifra ni autentica las credenciales.
- `wifiA/wifiB` se alternan dentro de `cielo96`. Solo se escribe la copia inactiva; commit y lectura por un handle independiente verifican el registro completo. Se vuelve a verificar el superviviente. Las variables de conexión cambian únicamente tras la verificación.
- Si la configuración ya coincide, se verifica y no se escribe, no se hace commit y no se desconecta. Se ensayan1000 repeticiones.
- Al arrancar se leen los nuevos registros. Si faltan ambos, se permite leer las claves antiguas y los valores de compilación, sin escribir una migración. El primer Guardar explícito crea el registro nuevo.
- Un registro de desactivación válido prevalece sobre las claves antiguas y los valores de compilación. Lecturas fallidas, esquema desconocido, generaciones contradictorias o ausencia de toda copia válida impiden recuperar silenciosamente una red antigua: queda la vía AP.
- No se borran claves antiguas, curvas, diarios, archivos ni particiones. No hay formato, reparación, compactación manual, init/erase de NVS ni cambios de tabla de particiones. Si NVS no tiene espacio, se informa del fallo.
- `WiFi.persistent(false)` precede a la primera inicialización de Wi-Fi. El SDK utiliza almacenamiento RAM y el registro de aplicación es la única ruta nueva de persistencia de credenciales. Esto no borra las copias antiguas del SDK.
- Se rechazan cambios Wi-Fi durante OTA o durante mantenimiento, incluida su comprobación inicial. Se conservan los controles de acceso existentes de los endpoints.
- `wifi_config` en status e Informe Maestro muestra backend, generación, fases, error, intentos y guardados verificados/sin cambios, incertidumbre y verificación del modo RAM. No contiene SSID, contraseña ni el blob. Los contadores son de la sesión de arranque.

La primera escritura deja una sola copia atómica válida (`degraded_copy=true`); la siguiente modificación verificada deja dos. No se duplica automáticamente el primer guardado para evitar escrituras adicionales. La configuración antigua se conserva como datos, pero no se usa si existe un registro nuevo ilegible.

## Límites y compatibilidad

Un fallo de commit o de lectura posterior puede ocurrir después de que el nuevo registro haya quedado persistido. En ese caso se devuelve error con `write_may_have_persisted=true`: la conexión de esta sesión se mantiene, pero un reinicio podría seleccionar el nuevo registro completo. No se asegura que el firmware anterior siga activo en flash después de cualquier error de escritura.

Una corrupción sintética que haga parecer el esquema desconocido provoca AP de recuperación, aunque exista una copia legible. Es una decisión de conservación; no se sobrescriben datos de esquema desconocido. Si la primera escritura deja contenido ilegible, también puede requerirse revisión por AP. Estos ensayos son simulados; no certifican cortes eléctricos reales ni el funcionamiento físico de NVS.

Una pareja coherente y verificada no prueba que la contraseña sea correcta para el router. No se implementa reversión automática por autenticación fallida. El AP `CIELO96-CONFIG`, la clave existente y la dirección192.168.4.1 se conservan. Su disponibilidad real depende de que Wi-Fi inicialice correctamente y se verifica en hardware.

**Volver a3.9 o una versión anterior:** ese firmware no lee `wifiA/wifiB`; utilizará las claves antiguas que se han conservado. Puede reaparecer una red anterior, incluso después de desactivarla en3.10. Tampoco se ofrece borrado seguro de credenciales. La pérdida total de ambas claves nuevas no puede distinguirse del primer arranque de esta función; no se garantiza la desactivación frente a daño completo de flash. Si la copia más reciente se corrompe, la copia anterior puede contener otra configuración habilitada.

El vigilante Sierra no cambia en esta versión. Se vuelven a ejecutar sus pruebas Python3.9. No se ha enviado un Telegram real ni verificado la V3 instalada en el iMac.

## Auditoría separada de las esperas de arranque

| Espera o condición actual | Efecto real | Decisión3.10 |
|---|---|---|
|30s desde boot, STA conectada y coexistencia válida | Retrasa la inicialización automática de BLE; HTTP/AP se sirven antes. Si falla STA, esta ruta automática no inicia BLE. | Conservada; medir con el próximo informe antes de reducirla. |
|180s de observación global | Condiciona salud de arranque, reanudación general y admisión de mantenimiento. En OTA exige validaciones adicionales. | Conservada; no confundirla con el tiempo de descubrimiento. |
|5s de scan cuando no se conoce dirección | Se espera el escaneo completo aunque el dispositivo haya anunciado antes. La dirección aprendida evita repetirlo en cada orden. | Posible optimización posterior mediante parada temprana; requiere revisar callback, vida útil de resultados y ambos dispositivos. |
|8s de conexión BLE y cuarentena de eventos tardíos | Acota connect sin reutilizar una operación SDK todavía pendiente. Servicio y escritura son otras fases. | Conservada; no se promete que toda operación BLE termine en8s. |
|OFF PV confirmado, comprobación cada300s | Puede haber una conexión cerrada intencionalmente entre comprobaciones. No implica luz físicamente verificada. | Conservada; presentación común3.9 distingue estado confirmado de telemetría reciente. |
|Ruta de rescate diurno45s, OFF previo y condiciones propias | Puede recuperar PV antes de la salud global, solo en rescate conocido y no en candidato OTA. | Conservada; no se generaliza a arranque normal. |

Estas esperas se solapan y dependen de las condiciones; no deben sumarse para dar un tiempo fijo de conexión. Un mantenimiento solicitado con comunicación antigua utiliza la comprobación de45s de3.8 antes de iniciar la rampa. Este documento identifica las causas, pero no declara resuelta la latencia de arranque.

Siguiente cambio del arranque: obtener marcas de inicio, Wi-Fi listo, radio BLE lista, scan, conexión, servicio, primer OFF y primer GATT de cada luz; comparar arranque normal, OTA y rescate. Después preparar una modificación acotada del scan, ensayando desconexión real, eventos tardíos y pérdida de Wi-Fi, sin retirar las puertas de seguridad ni la validación OTA.

## Verificación reproducible y prueba física pendiente

`python tools/check_b89.py` integra tres pruebas nuevas: registro/selección, adaptador NVS real con API simulada y ejecución de los handlers reales extraídos de main.cpp. Cubren límites, esquemas/generaciones, errores de cada lectura, NoSpace, commit incierto, verificación corrupta,112 interrupciones sintéticas de reemplazo, no-op1000, desactivación, lectura legacy sin escrituras y exclusión OTA/mantenimiento. Se comprueba que las claves ajenas queden intactas. No representan112 cortes eléctricos en un ESP32.

El recorrido Chromium añade un507 Wi-Fi: el panel conserva SSID y sigue abierto para corregir/reintentar. Continúa la revisión de controles, estados comunes, guardado de curvas, mantenimiento y OTA a320/390/768px con servidor simulado. Los hashes de la suite de conservación fijan motor, BLE, seguridad, mantenimiento y almacenamiento de curvas. Actions también compila el SDK ESP32-S3 real y prueba Sierra con Python3.9. Resultado concreto y huellas del BIN se documentan en la entrega, después de terminar CI.

Para validar en el ATOM: instalar solo aplicación por OTA; comprobar perfil LittleFS gen10/38PV/32CH y estados Luz/Centro; descargar Informe Maestro. Al probar Wi-Fi, disponer de la clave del AP privado y trabajar en casa. Guardar la red actual crea la primera copia; repetir la misma petición no debe aumentar escrituras ni desconectar. La prueba de cambiar/desactivar la red se hace de forma deliberada por AP y requiere volver a verificar acceso tras reinicio. No es necesario cambiar de red para usar la iluminación de esta candidata.

## Fuentes técnicas primarias

- Arduino-ESP32 **2.0.17**, `WiFiGeneric.cpp`: `_persistent=true` por defecto; `persistent(false)` y `wifiLowLevelInit` seleccionan/checkean `WIFI_STORAGE_RAM`. `WiFi.mode` comunica el fallo de inicialización: https://github.com/espressif/arduino-esp32/blob/2.0.17/libraries/WiFi/src/WiFiGeneric.cpp
- Misma versión, `WiFiSTA.cpp`: begin configura la red; `disconnect(false,false)` conserva configuración y radio; no se usa setAutoConnect obsoleto: https://github.com/espressif/arduino-esp32/blob/2.0.17/libraries/WiFi/src/WiFiSTA.cpp
- ESP-IDF **4.4.7**, NVS: tipos, commit y errores que pueden aparecer tras una escritura. Las garantías y fallos físicos del SDK se distinguen de la transacción y sus pruebas host: https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32s3/api-reference/storage/nvs_flash.html
