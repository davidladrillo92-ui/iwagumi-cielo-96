# RECOVERY3.5.2: guardado NVS con SPIFFS no disponible

## Evidencia física nueva

Informe Maestro 78, 26/10/2026 17:54:57 Europe/Madrid:

- Firmware instalado RECOVERY3.5.1-FS-GUARD-CANDIDATE, arranque 130.
- `fs_available=false`, `fs_status=UNMOUNTABLE_NONEMPTY_PRESERVED`.
- Guardado por NVS legado: `curvesA`, 788 bytes solicitados, 0 aceptados,
  `4357 / ESP_ERR_NVS_NOT_ENOUGH_SPACE`, fase `escribir`.
- NVS: 474 entradas usadas, 156 libres estadísticas, 630 totales.
- Continúa generación 8, 34 puntos PV y 31 CH. La V5 38/31 no se guardó.
- Ambos enlaces BLE conectados, cero errores BLE de este arranque,
  sin bloqueo actual; esto no verifica emisión física de luz.

La 3.5.1 funcionó como protección, pero su solución de almacenamiento no
cubría este ATOM: SPIFFS no montable y no vacío impidió usar los archivos,
y el fallback ejecutó la misma escritura NVS que fallaba en 3.4. No es una
prueba de corrupción física ni identifica qué datos hay en SPIFFS. El
informe no contiene las páginas NVS ni las generaciones de ambos slots.

## Corrección

Mantener SPIFFS intacta cuando no pueda montarse. No formatear, reparticionar
ni borrar NVS completa. Conservar el transporte BLE persistente de 3.3 y
las protecciones de SPIFFS de 3.5.1.

Para el fallback NVS:

1. Leer ambos slots mediante API NVS con errores exactos y CRC de perfil.
2. Elegir el slot inactivo; verificar por lectura independiente que el otro
   coincide byte a byte con el perfil activo en RAM.
3. Intentar primero `nvs_set_blob` normal, sin borrar ninguna copia.
4. Solo ante `ESP_ERR_NVS_NOT_ENOUGH_SPACE`, con dos copias válidas, releer
   la activa y retirar exclusivamente el slot redundante antiguo.
5. Confirmar esa retirada, volver a verificar la activa e intentar una
   única escritura del nuevo perfil. Si falla, detenerse; no tocar otras claves.
6. Confirmar y releer los 788 bytes; activar en RAM solo si validación y
   comparación completas son correctas.

Mientras se reemplaza el respaldo puede quedar una sola copia persistente
válida: la activa. La recuperación no elimina esa clave. Si solo existía
una copia válida, no se ejecuta la retirada del respaldo. Si la lectura de
la activa no coincide o falla, tampoco se continúa. No se garantiza que
liberar un respaldo baste en todas las distribuciones de páginas NVS.

El SDK puede publicar el blob nuevo y después devolver un error al retirar
su versión anterior. Los errores distintos de NoSpace posteriores al intento
se comunican conservadoramente como guardado posiblemente persistido. Un
fallo de commit/verificación tampoco se presenta como ausencia de escritura.

Se eleva la reserva para journals no críticos a 188 entradas estadísticas
(126 de trabajo GC, dos blobs mínimos de 27 y 8 de margen). Es una política
conservadora, no una predicción de espacio físico. Los registros de reinicios
mantienen su política existente para no perder esa observación prioritaria.

Nuevo diagnóstico de cada slot: error de lectura, validez, generación y
coincidencia con la activa, sin contenido del perfil ni otras claves. También
se informa intento de recuperación, respaldo retirado, error inicial, escritura
aceptada, posible persistencia y verificación final. `nvs_legacy_preserved`
describe si el último intento mantuvo ambos slots, no promete que el respaldo
antiguo permanezca después de una recuperación NVS.

## Pruebas

- Ejecutar transacción de producción contra modelo de asignador con 474/156
  entradas. La distribución por páginas es sintética, no extraída del ATOM.
- Cortes simulados entre llamadas; errores de lectura, CRC, commit, escritura,
  error posterior a publicación, una sola copia válida y generaciones wrap.
- Ejecutar las funciones reales `saveProfile` y `CurveNvsStore`, extraídas del
  firmware, con SPIFFS no disponible y API NVS simulada. Comprobar éxito del
  reintento, fallo doble, lectura defectuosa, estadísticas fallidas, commit
  incierto y protección de la generación activa.
- Perfil V5 exacto checksum c4d2d529: 38 PV, 31 CH, Chihiros idéntica; lectura
  y selección de arranque simuladas.
- Suite completa de host, build ESP32-S3 y pruebas Sierra Python 3.9 en CI.
  El resultado final de Actions se documentará en el PR de entrega.

Estas pruebas no prueban cortes eléctricos físicos ni reconstruyen la
distribución interna actual. Validación en ATOM pendiente.

## Verificación local por el propietario

Conservar antes la exportación del perfil vigente y la V5 deseada. Instalar
solo el BIN de aplicación con identificación 3.5.2-NVS-RECOVERY. No usar una
imagen de filesystem ni borrar flash. Importar V5, guardar una vez y descargar
el informe. Éxito esperado: generación siguiente, 38 PV / 31 CH,
`last_curve_stage=correcto` y `save_verified=true` en la vía NVS. Si necesita
recuperar espacio: `recovery_attempted=true`, `inactive_slot_reclaimed=true`.
Que SPIFFS continúe no disponible es compatible con este guardado por NVS.

Tras éxito, exportar y comparar el perfil, reiniciar de forma controlada y
verificar que persisten generación y puntos. Si hay error o resultado incierto,
conservar el editor/exportación y descargar informe antes de repetir.

## Referencia de implementación SDK

Espressif ESP-IDF v4.4.7, `components/nvs_flash/src/nvs_storage.cpp`,
`Storage::writeItem`: escribe la nueva versión del blob antes de retirar la
anterior; puede devolver `ESP_ERR_NVS_REMOVE_FAILED` tras publicación. Es
referencia de la rama 4.4 usada por Arduino-ESP32 2.0.17, no una lectura de flash.

https://github.com/espressif/esp-idf/blob/v4.4.7/components/nvs_flash/src/nvs_storage.cpp
