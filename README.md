# IWAGUMI CIELO — RECOVERY 3.15 CANDIDATE
ATOMS3 Lite C124 / ESP32-S3 · control local, autónomo, sin vigilante externo.
Base física auditada: RECOVERY3.12, commit c2cb7fedcc0cc887f003f0c84bcac0a0b8748f39; informe de 20/11/2026 (boot 149, 672727 s, 16849 operaciones GATT sin error definitivo). No demuestra resultado óptico ni prueba física de 3.13.

- [Auditoría y cambios 3.13](docs/RECOVERY3_13_AUTONOMIA_20261120.md)
- [Auditoría 3.12](docs/AUDITORIA_DIAGNOSTICO_3_12_20261031.md)
- [Respaldo USB privado](docs/USB_RECUPERACION.md)

Motor, perfiles, BLE worker, mapa de particiones y política LAN/OTA conservados. El servicio de vigilancia Mac/Telegram/MQTT y sus rutas se retiran de esta rama; fuentes antiguas siguen recuperables en 3.12. No se borra configuración NVS antigua ni se instala/desinstala nada en el iMac.

```sh
python3 tools/check_b89.py
python3 -m platformio run -e atoms3_lite_c124
```
El CI genera aplicación BIN, SHA256, manifiesto y fuentes. No instalar sin backup/exportación actual y validación física supervisada. Ninguna escritura GATT certifica luz óptica.

## Recovery 3.15 DEV2 (desarrollo)
Ver `docs/RECOVERY3_15_DEV2_FINAL_20261126.md` y `docs/VALIDACION_DEV2_FINAL.json` para alcance, dos revisiones y resultados reproducibles. Fuentes de desarrollo: no hay BIN ni validación objetivo/física. No instalar en el acuario.

## Recovery 3.15 DEV3 FINAL
Ver `docs/RECOVERY3_15_DEV3_FINAL_20261126.md` y `docs/VALIDACION_DEV3_FINAL.json` (93/93 comprobaciones host). Repetir con `python3 tools/run_dev3_validation.py /tmp/validacion-dev3.json`. No hay BIN, compilación ESP32-S3 ni ensayo físico. Mantener 3.14 V2 instalada.

## Candidate 27/11/2026
Integra DEV1 REV3, DEV2 FINAL y DEV3 FINAL de forma acumulativa. Compilación y navegador mediante CI, hardware pendiente. No se habilita el ensayo óptico nocturno sin validación física.
