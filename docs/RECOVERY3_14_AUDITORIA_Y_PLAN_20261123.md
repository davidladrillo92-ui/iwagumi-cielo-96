# RECOVERY 3.14 — auditoría de arranque y plan de autonomía (23/11/2026)

Base: 3.13 commit 3acc5b6; perfil V5 de propietario: generación LittleFS 22, 37/46 puntos, checksum c1960ba7. No se cambia el motor de curvas, protocolo GATT, tabla de particiones, prioridad nocturna OFF ni la política de rollback OTA. El vigilante Mac continúa eliminado.

## Evidencia del Informe Maestro boot 150, 23/11/2026 18:17:28

Uptime 609s. PV GATT 9/9, CH 8/8, errores definitivos 0. PV max operación 5785ms, CH 6722ms; fase connect PV 121ms y CH 39ms, escritura individual PV max 3ms, CH max 146ms. Los dos avisos de timeout blando >3500ms no implican fallos de transmisión: medir preparación, cola y retorno, y diferenciar primer enlace de operaciones estables. Reanudación general V5 a 180009ms, primer GATT diurno ~186743ms, OTA validada ~201757ms. Es intencionada la ventana de observación de 180s.

## Primera implementación — instrumentación sin cambios de comportamiento

Se incorpora boot_timing al Informe Maestro: ms desde arranque hasta BLE inicializado, primera operación GATT aceptada por lámpara, validación base de arranque y reanudación de ambas curvas. Son evidencias de software, no medición óptica. Los valores no observados son null. No se adelantan luces ni se cambian watchdog/rollback en este commit.

## Desarrollo posterior por etapas, con pruebas previas

1. Descomponer duración BLE en espera de cola, inicialización, discovery, conexión, escritura y respuesta. Medir percentiles tras el arranque y no penalizar la primera conexión como error persistente. Mantener los avisos de 3,5s como diagnósticos históricos, no avería.
2. Recuperación rápida de curvas con hora fiable, perfil validado, BLE sano y prioridad OFF nocturna. Mantener la observación OTA de 180s y la validación GATT posterior separadas. No adelantar si el reloj no es fiable, si hay OTA, pausa voluntaria, rescate o interbloqueo PV. Nunca dar por confirmado el efecto óptico.
3. Autonomía BLE sin router: intentar coexistencia radio en AP+STA y arrancar BLE aunque no conecte la Wi-Fi doméstica; no permitir una hora desconocida como motivo para iluminar fuera de horario. Probar router ausente, retorno del router y reinicio con noche.
4. Endurecer sincronización de reloj y retorno 0x03 con IDs de solicitud, rechazo de callbacks tardíos y pruebas de pausa/retorno.
5. Reducir memoria temporal de exportación JSON por streaming solo tras verificar igualdad byte a byte del esquema, y evaluar persistencia histórica LittleFS con escrituras acotadas.

## Puertas de aceptación

Compilación ESP32-S3 C124, validaciones de perfiles y flash, pruebas de LittleFS ante corte, UI navegador, OTA/rollback, curva V5 intacta, comprobación de OFF de Pez Verde, CH retorno, arranque sin Wi-Fi y sin reloj. Ensayo físico 24-48h antes de sustituir 3.13. No afirmar certificación óptica con GATT.
