# IWAGUMI CIELO 96

Control local de iluminación para M5Stack ATOMS3 Lite C124 / ESP32-S3.

**Esta rama: RC5 candidata B1–B9, no instalada.** Base PR #33
`4492f221f7d73cea796cb12b4acc0a819ff67486`. La instalación real confirmada sigue RC3.
La V5 de referencia PV34/CH31 se conserva; no se importa automáticamente.

- [Entrega B8/B9 y criterios de aceptación](docs/B8_B9_RC5.md)
- [Observador opcional, avisos e historial](monitor/README.md)
- [Respaldo privado y recuperación USB](docs/USB_RECUPERACION.md)
- [Auditoría de mantenimiento](docs/MANTENIMIENTO.md)

```sh
python3 tools/check_b89.py
python3 -m platformio run -e atoms3_lite_c124
```

CI genera BIN candidato, fuente completa y SHA256. La firma ECDSA requiere la clave privada
original fuera del repositorio. No instalar sin copia real de 8 MiB, firma y pruebas físicas.
No dejar firmware nuevo sin supervisión durante la ausencia del viernes 23/10/2026.
GATT aceptado no demuestra luz física. No subir nunca NVS real ni contraseñas al repositorio.
