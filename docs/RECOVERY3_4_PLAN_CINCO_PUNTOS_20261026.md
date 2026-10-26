# RECOVERY3.4 — Consolidación de cinco puntos (borrador técnico)

Fecha: 2026-10-26. Base: RECOVERY3.3 BLE persistente, Informe Maestro 75 (14:49:45), boot 128.

**Estado:** trabajo en rama aislada, no instalar ni fusionar. El firmware instalado RECOVERY3.3 no se modifica.

## Evidencia de partida

- 9 h 7 min, dos conexiones persistentes (una por pantalla), 124/124 GATT Pez Verde y 545/545 Chihiros, 0 desconexiones inesperadas.
- Error de guardado de curvas: `curves.last_error = "NVS escritura incompleta"`, generación 8, 34 puntos PV, 31 CH. La nueva curva de 38 puntos **no quedó guardada**.
- Heap libre ~99.984 B, mínimo 54.220 B, bloque libre mayor 90.100 B. El mínimo de heap por sí solo no prueba fuga.
- Historial: boot 127 `PENDING` aunque boot 128 figura recuperado; boot 124 con epoch futuro de 2029. No reescribir la evidencia histórica.
- El GATT aceptado no prueba resultado óptico.

## P0 — Punto 5 añadido: almacenamiento NVS de curvas

**Primera implementación en esta rama:**
- Lectura no destructiva de `nvs_get_stats("nvs")` y exposición de entradas libres/usadas/totales y namespaces en `nvs_diagnostics`.
- Registrar en RAM slot elegido, tamaño solicitado, tamaño devuelto por `Preferences.putBytes` y contador de fallos por arranque.
- Conservar el esquema de doble copia `curvesA/curvesB`, verificación por lectura y CRC, sin borrar NVS, claves, datos ni particiones.
- El envoltorio `Preferences.putBytes` solo devuelve longitud, **no el esp_err_t real**. No afirmar que conocemos el código de error hasta instrumentar el SDK de forma controlada.
- Próximo paso condicionado a datos: elegir estrategia de escritura atómica y recuperación que preserve las dos copias existentes, o reducir presión de escrituras no críticas. Nunca ejecutar `prefs.clear()` como reparación.

**Aceptación:** importar 38 PV + 31 CH, guardar, exportar, reiniciar de forma controlada, releer idéntico perfil y comprobar rollback ante error simulado. También repetir 10 guardados alternando slots y comprobar que no afecta BLE.

## P1 — Recuperación de Chihiros

**Estado:** sin fallo real de desconexión en el arranque 128, no validado físicamente.

- Simular pérdida de enlace Chihiros en entorno de pruebas y comprobar que el retorno `0x03` solo se envía si BLE vuelve a estar operativo.
- Garantizar liberación del enlace para que My Chihiros pueda conectarse.
- Mantener Pez Verde bajo control si el fallo es exclusivo de Chihiros; revisar interbloqueo global antes de cambiarlo.
- Medir latencia, reintentos, éxito GATT y confirmación visual por el usuario. No declarar retorno óptico sin observarlo.
- No cambiar curvas V5.

## P1 — Recuperación diurna de Pez Verde

**Estado:** el Informe 75 incluye una muestra diurna, 17 órdenes de PV y evento `PV_DIA_REANUDADO`; no hay evidencia de una desconexión diurna real.

- Probar pérdida/reconexión mientras PV está encendida, garantizar orden OFF prioritaria cuando corresponda y no reproducir RGB antiguos.
- Distinguir desconexión de Chihiros de fallo de PV y de fallo global; no apagar PV indiscriminadamente por un fallo aislado de Chihiros sin justificar la política.
- Verificar el efecto visual con presencia del usuario, nunca a ciegas durante la noche.

## P1 — Memoria mínima

**Primera implementación:** registrar mínimo del bloque contiguo libre muestreado cada 15 minutos en RAM; no escribir muestras adicionales en NVS.

- Contrastar heap actual, mínimo histórico, bloque contiguo mínimo muestreado y fragmentación.
- Probar 24–72 h y cargas de importación, OTA y retorno a My Chihiros.
- Umbrales de advertencia solo tras datos; evitar reinicios preventivos por un único mínimo.

## P1 — Historial de reinicios y reloj

**Primera implementación:** enriquecer `recent_restarts` sin alterar entradas persistidas:
- `boot_epoch_quality`: `MISSING`, `FUTURE_UNTRUSTED` o `UNVERIFIED_HISTORICAL`.
- `following_boot_recovered`: evidencia de que el arranque inmediatamente siguiente reportó recuperación; no cambia el estado histórico `PENDING` del arranque original.
- No inventar fechas para boot 127 ni corregir retrospectivamente 2029 con una hora inferida.

**Aceptación:** informes coherentes después de reinicio con NTP tardío y pruebas de hora incorrecta.

## Restricciones innegociables

No cambiar puntos ni horarios V5, transporte Bluedroid, conexiones persistentes, prioridad de apagado, rollback OTA, monitor iMac Sierra, Telegram ni NVS existente. Ningún ensayo de desconexión física o instalación sin autorización del propietario. GitHub CI valida compilación y pruebas de host; no sustituye la prueba física.
