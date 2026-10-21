# IWAGUMI CIELO RC5 — Bloques 2 y 4 · entrega de banco
21/10/2026 · rama `dev/cielo-11-1-rc5-b1-b5-b3-b2-b4-curves-20261021`

## Integración
Base de RC5 B1+B5+B3 R2. Mantiene HTTPS/PBKDF2/sesiones, OTA firmada, apagado priorizado Pez Verde y diagnóstico v3. **No altera los puntos ni colores V5 del ATOM real**, que siguen en NVS. `profiles_generated.h` permanece idéntico a la base.

### B2 — Motor
- Hasta **64 puntos por pantalla**, límites verificados en C++ y editor. Horas siguen en minutos desde medianoche, sin segundos editables y sin dos puntos con el mismo minuto.
- Frecuencia base **30 segundos** (misma interpolación por minuto que la RC5 anterior). **15 segundos** opcional desde el editor: interpolación subminuto `atSecond` para escalones más finos, solo en RAM; vuelve a 30 s tras reboot o si aparece watchdog / 2 errores GATT / 2 soft timeouts durante el ensayo.
- **La luna V5 salta directamente** de `22:59 2/2/2` a `23:00 1/1/2`, sin colores intermedios cuando el perfil tiene exactamente esos puntos.
- Una cola por dispositivo, un trabajador BLE; las consignas pendientes se superseden al cambiar el RGB para evitar aplicar colores antiguos tras un retardo. `POWER OFF` de B1 prevalece cuando brillo=0. Diagnóstico B5 permite monitorizar colas, GATT, RAM y tiempos por fase.

### B4 — Guardado
- Editor guarda **siempre las DOS curvas juntas** vía POST admin protegido `/api/profile/transaction`, incluyendo curvas `pez`, `chi`, `generation` (optimistic concurrency para evitar sobrescribir otro cambio), `crc32` de `pez_serializada + LF + chi_serializada`.
- Validación completa previa de ambos canales (2..64 puntos, horas únicas ordenadas, minuto final 1440, Chihiros RGB final 0, Pez Verde brillo final 0, valores RGB correctos). Respuesta de error HTTP 400/409/422/507 no modifica la copia RAM activa del perfil.
- Persistencia de las DOS curvas dentro de **una única ProfileImage**, con CRC/FNV y lectura posterior; escritura a slot NVS alterno `curvesA` / `curvesB`. Solo un perfil operativo: la otra copia es **rescate técnico**, no biblioteca de perfiles.
- Endpoints de guardado individual devuelven HTTP 410: imposible que interfaz antigua guarde un canal de la pareja por separado.
- JSON import/export `schema_version:2`, `timezone:'Europe/Madrid'`, `time_unit:'minute'`, `checksum CRC32`. Se permite importar V5 heredado sin checksum con advertencia y comprobación de rangos. No se guarda al importar hasta pulsar «Guardar las dos».
- Deshacer/rehacer hasta 20 estados del editor, también para importar, crear, modificar, eliminar y restaurar originales; el historial vive en el navegador, no en NVS.

## Pruebas automatizadas y límites
- `tests/test_b2_b4.cpp`: paridad 30s a cada minuto; valores subminuto 15s, luna sin intermediarios, 64 puntos, comprobación de integridad de imagen, dos slots con corrupción simulada, cola BLE y supersesión de última consigna.
- `tests/test_b2_b4_browser.js`: JSON real V5 con 34 PV/31 CH, CRC32 test vector, límite 64, un solo POST atómico, conflicto de generación conserva ediciones, importación heredada y rechazo de checksum alterado.
- `tests/test_b2_b4_source_contract.py`: inspección estática de conexiones y exclusión de antiguas rutas de escritura.
- `tools/analyze_v5_15s.py`: estimación de cambios de RGB durante 14:30-15:40 y 21:50-24:00. **No equivale a medición de respuesta del ESP32 ni de luminosidad**.

## Criterio de aceptación física (NO realizado)
1. Copia flash RC3 **privada** por USB, hash SHA256, sin subir claves WiFi a GitHub; comprobar recuperación.
2. Instalar BIN firmado solo bajo supervisión con OTA preflight + permiso físico; verificar Safari HTTPS y restauración My Chihiros.
3. Guardar ambas curvas y cortar alimentación al ATOM únicamente **en banco** tras asegurar las luces y mantener equipos vitales encendidos; reabrir NVS y validar perfiles iguales y generación.
4. Intentar guardado de JSON incorrecto, edición concurrente y reinicio durante guardado; comparar reportes y confirmar V5 original sin cambios.
5. Probar **30s, luego 15s durante amanecer/atardecer**, medir heap mínimo, latencias GATT, colas y errores; comprobar retorno automático a 30 al degradarse.
6. Verificar POWER OFF nocturno real de Pez Verde, incluido en pérdida Bluetooth y retorno de luz, **sin confundir GATT OK con confirmación óptica**. Si ATOM o BLE queda fuera de servicio, hace falta temporizador eléctrico independiente probado.

**Estado:** candidato de desarrollo comprobable en GitHub; no apto para dejar sin vigilancia antes del viaje del viernes sin pruebas de hardware y respaldo USB.
