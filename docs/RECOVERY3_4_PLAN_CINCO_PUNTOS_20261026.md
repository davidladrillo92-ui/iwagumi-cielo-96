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

## Implementación RECOVERY3.4 — segunda iteración

- **Escritura NVS con error exacto:** sustitución localizada de `Preferences.putBytes` por `nvs_open`, `nvs_set_blob`, `nvs_commit` y `nvs_get_blob`, manteniendo la misma partición y namespace `cielo96`. El Informe Maestro ahora muestra `last_curve_error_code`, `last_curve_error_name` y `last_curve_stage`. No se ha medido todavía el error real del dispositivo: se verá tras instalar y repetir un guardado.
- **Doble copia protegida:** inspección de `curvesA/curvesB` antes de escribir. Nunca sobrescribe el único slot válido; si ambos son válidos, sobrescribe el más antiguo. Perfil RAM solo cambia después de `commit` + lectura + comprobación de checksum + igualdad byte a byte.
- **Preflight NVS:** se consulta `nvs_get_stats` y se rechaza solo si no hay ni el mínimo teórico de entradas para un blob de `sizeof(ProfileImage)`. Esta estimación es una cota inferior, no garantiza espacio contiguo ni ausencia de fragmentación; el resultado de `nvs_set_blob` es autoritativo.
- **Reducción de escrituras:** el diario de diagnóstico se mantiene en RAM y omite volcado si el espacio es escaso; `boot_hist` redundante también se omite bajo presión. El historial principal `restart_v2` conserva su prioridad. Los contadores de omisiones aparecen en el informe. No se borran entradas antiguas ni se hace `nvs_erase_all`.
- **Mensajes específicos:** se distingue perfil inválido, falta de estadísticas, insuficiencia de entradas, error de apertura, escritura, confirmación, lectura y verificación. HTTP 507 solo para falta de espacio; HTTP 500 para otros fallos. El perfil anterior permanece en RAM.
- **Desconexiones BLE independientes:** desconexión de Chihiros no impone POWER OFF a Pez Verde. La desconexión de Pez Verde sí exige OFF GATT antes de reanudar RGB diurno. Chihiros continúa con retorno personalizado 0x03 y liberación del enlace a My Chihiros.
- **Pruebas:** tests C++ de política NVS, escenarios host de recuperación PV/CH, aserciones de integración y compilación PlatformIO en GitHub Actions.

**Límites:** un fallo de flash/NVS persistente puede impedir cualquier nuevo guardado; no se puede prometer que RECOVERY3.4 arregle el dispositivo sin diagnosticar la causa física. Ningún GATT verifica el estado óptico. Las pruebas de desconexión diurna requieren supervisión del usuario. OTA debe realizarse con respaldo del firmware y del perfil existente.
