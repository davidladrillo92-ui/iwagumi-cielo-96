# IWAGUMI CIELO — RECOVERY3.1 integrada (candidata)

## Informe Maestro 69, 25/10/2026 22:31:22
- Firmware real: 11.1.0-RC5-RECOVERY3; boot 123, uptime 5665 s (94 min 25 s).
- Estado: avisos históricos; incidencias críticas actuales: false.
- Reinicio 122→123: supervisor BLE, Chihiros, fase CONEXION, 120000 ms; recuperado.
- Wi-Fi: -36 dBm, cero caídas. Heap libre 109336 B, mínimo 66732 B, bloque mayor 94196 B.
- Pez Verde: 95/95 operaciones GATT, 0 fallos; p95 2045 ms; peor operación 6455 ms.
- Chihiros: 92/92 operaciones GATT, 0 fallos; p95 1837 ms; peor operación 2547 ms.
- 3 OFF GATT de Pez Verde a ~36, 98 y 159 s; V5 reanudada a ~180 s.
- Perfil V5 generación 8, PV 34 puntos, CH 31 puntos, sin recuperación por defecto.
- Cero muestras nocturnas. No existe confirmación óptica.
- Las pantallas sueltan deliberadamente el enlace BLE tras cada transacción: connected=false e idle_released=true no implican fallo.

## Cuatro bloques acumulados
1. Clasificación: distingue worker bloqueado, espera GATT y conexión pendiente SDK, sin afirmar la causa exacta de callbacks.
2. Aislamiento Chihiros: solo cuando limpieza SDK CH pendiente y worker vivo; evita liberar clientes BLE desde HTTP; rescate global acotado sigue disponible.
3. Recuperación diurna PV: en rescate BLE y no en OTA, tras 45 s, OFF GATT confirmado, hora/perfil/Wi-Fi/coexistencia válidos y worker sano, PV puede reanudar consigna actual antes de CH. No omite el OFF inicial. El apagado nocturno sigue siendo prioritario.
4. Diagnósticos: 16 últimas transiciones de fase RAM; retención RTC de las 16 anteriores al reinicio supervisado; callbacks reales de conexión/desconexión, tiempos, diagnósticos de bloqueo y tests host de estados adversos.

## Interfaz
Luz y Centro muestran **Conectado** solo con GATT reciente y enlace activo o liberado intencionadamente. La palabra no significa conexión BLE permanente ni prueba de luz física. En errores se muestra Reconectando, Sin respuesta o Sin comunicación reciente.

## No alterado
Curvas V5 (PV 34 puntos y CH 31), calendario, My Chihiros retorno personalizado 0x03, monitor Telegram Sierra, API de Informe Maestro, esquema NVS de perfiles.

## Riesgos y validación obligatoria
- Los tests host de callbacks tardíos son modelos de decisiones, no inyección en Bluedroid real.
- El callback real ON_CONNECT/DISCONNECT se cuenta, pero todavía no se captura cada evento interno REG/OPEN/SEARCH del SDK.
- El RTC persiste en reinicios software; no garantiza persistencia tras pérdida de energía.
- El arranque inicial mantiene OFF GATT; el objetivo es reducir el tiempo apagado de ~180 s a una recuperación posible desde ~45 s, sin prometerlo si BLE está bloqueado.
- La edición LAN mantiene HTTP sin autenticación/HTTPS; este cambio no pretende solucionar seguridad OTA.
- Antes de instalar: CI host, compilación PlatformIO y comprobación de memoria; después, pruebas físicas de día, noche, OTA y callback tardío, con posibilidad de rollback.
- Estado: candidata, NO fusionada ni validada en hardware.
