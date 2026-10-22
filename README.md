# Candidata actual: RC5 SIMPLE1

Portada compacta, un solo informe descargable y control automático sin botón inicial. Conserva acceso LAN sin claves, V5 PV34/CH31 y recuperación BLE. [Cambios, análisis del informe LAN1 y ensayo](docs/SIMPLE1_INTERFAZ_AUTONOMIA.md). Última instalación confirmada: LAN1, 12 minutos en el primer informe; SIMPLE1 pendiente de prueba física.

# Candidata actual: RC5 LAN1

Acceso local HTTP sin contraseñas, clave OTA, firma obligatoria ni pulsación física para actualizar, por decisión expresa del propietario. Corrige también la espera posarranque y evita iniciar HTTPS. [Uso, migración desde RC3 y ensayo físico](docs/LAN1_ACCESO_SIMPLE.md). V5 PV34/CH31 intacta. RECOVERY y STACKFIX1 quedan retiradas del ensayo.

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

### Revisión de bloqueo BLE de RC3

Candidata actual: **11.1.0-RC5-B1-B9-RECOVERY**. [Incidente 41/42 y límites de recuperación](docs/INCIDENTE_41_42_Y_RECOVERY.md). [Copia USB e instalación desde iPhone](docs/INSTALACION_RC3_A_RC5_IPHONE.md). No instalada ni validada físicamente.

### Corrección actual de primer arranque

**11.1.0-RC5-B1-B9-STACKFIX1** sustituye a RECOVERY, cuyo desbordamiento de pila de loopTask se confirmó físicamente. [Evidencia, corrección y ensayo](docs/STACKFIX1_ARRANQUE_TLS.md). Última instalación confirmada RC3; no reinstalar el BIN RECOVERY anterior.
