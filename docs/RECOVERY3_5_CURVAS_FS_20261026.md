# IWAGUMI CIELO · RECOVERY3.5 — Almacenamiento persistente de curvas

Fecha: 26 octubre 2026 · Candidata para ATOMS3 Lite C124 (ESP32-S3)

## Causa técnica verificada

Informe Maestro 77, boot 129, RECOVERY3.4:
- nvs_set_blob(curvesA) rechazó perfil completo de 788 B.
- Error ESP_ERR_NVS_NOT_ENOUGH_SPACE (4357 / 0x1105).
- NVS 474 used / 156 free / 630 total: el agregado no garantiza páginas para blobs.
- Generación 8 en NVS no fue alterada.
- BLE Pez Verde y Chihiros persistentes.

## Comparación histórica

RECOVERY3.3, 3.2, 3, RC5 y RC3 también usaban Preferences y claves de
perfil en la NVS de 20 KiB. RECOVERY3.4 añadió diagnóstico y selección segura,
pero no eliminó la dependencia del espacio NVS.

## Implementación

- include/cielo_profile_fs.h: store con dos slots de perfil verificables
  /cielo_profile_a.bin y /cielo_profile_b.bin, y temporal /cielo_profile_tmp.bin.
- Partición SPIFFS existente: 0x670000 y 0x180000 (1,5 MiB).
- Sin cambios en tabla de particiones, curvesA y curvesB de NVS permanecen intactas.
- Montaje SPIFFS no destructivo. Solo si CADA byte de la partición es 0xFF
  se puede inicializar el sistema de archivos en blanco. Si contiene datos
  no montables, se preservan y permanece el fallback NVS legado.
- Escritura: temp -> flush -> close -> validación CRC + bytes ->
  eliminar únicamente el slot antiguo/inactivo -> rename -> relectura.
  Solo después de verificación total cambia el perfil de iluminación en RAM.
- Inicio: elegir la generación válida más reciente entre NVS y SPIFFS.
- Bluetooth, V5, retorno a My Chihiros, OTA y monitor Sierra sin modificar.

SPIFFS no es transaccional perfecto bajo pérdida física de alimentación.
Los dos slots protegen la copia previa durante la sustitución normal, pero no
son garantía absoluta contra corrupción simultánea. Mantener backup USB.

## Diagnóstico y aceptación

- En informe curve_storage.backend: SPIFFS_AB cuando está disponible.
- fs_available debe ser true y fs_status muestra el montaje.
- Tras guardar: last_curve_stage = SPIFFS_VERIFICADO y generación 9,
  38 puntos PV y 31 CH, exportación idéntica después de reinicio.
- Si fs_status = UNMOUNTABLE_NONEMPTY_PRESERVED: no borrar ni formatear.
- Antes de instalar: exportar curva activa y conservar firmware RECOVERY3.4.
- Instalar de día, comprobar ambas luces y datos de almacenamiento, y entonces
  importar CIELO_V5_AMANECER_AZUL_1400_PV_25_35_25.json.
- Probar 10 guardados/relecturas, reinicio, errores y BLE/heap durante 24–72h.
- CI y simulación host no sustituyen las pruebas físicas en el ATOM.
