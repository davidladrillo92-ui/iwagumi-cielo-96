# RECOVERY3.8 — disponibilidad nocturna y mantenimiento sencillo

Base exacta: RECOVERY3.7 d78e6aef0634086c91769f1719b741cdaddf8b45 (PR60).
Nueva candidata:11.1.0-RC5-RECOVERY3.8-MAINTENANCE-CANDIDATE.

## Evidencia y causa

Informe89,27/10/2026 10:19:09, boot138, gen10/38PV/32CH: el usuario confirma que mantenimiento funcionó visualmente. Los eventos registran inicio1844501ms, meseta2444505ms, retorno4253185ms y fin4373187ms.253 ticks,104/104 operaciones PV y127/127 CH correctas,0 errores y0 pérdidas de enlace. Al terminar, OFF GATT confirmado;126s después la UI muestra «Sin verificar» y bloquea mantenimiento porque exige90s de frescura mientras OFF se comprueba cada300s. No es un fallo de escritura ni evidencia de apagado óptico medido.

## Comportamiento

- Se conserva el flag estricto online_recent90s. Se añade pv_waiting_off_check: último OFF GATT confirmado, sin errores, enlace conectado y trabajador vivo, sin interlocks, y aún dentro del intervalo300s desde ese OFF. UI muestra «En reposo»; no lo presenta como nueva confirmación de comunicación ni como medición de luz.
- Diagnóstico profesional considera ese reposo programado correcto. Si vence300s, falla el enlace/trabajador o hay error, no se aplica la excepción. El tooltip explica que el OFF es la última confirmación y hay una siguiente comprobación programada.
- Iniciar solicita comprobación asíncrona de AMBAS consignas actuales mediante el scheduler existente: por la mañana PV recibe OFF, nunca un destello de prueba; CH mantiene su consigna actual. Se fuerza una comprobación OFF adicional solo por solicitud del usuario. La cadencia automática sigue300s.
- Hasta que ambos contadores GATT de consigna avancen respecto a la solicitud, sean recientes y no haya operación en curso, no empieza la rampa. Un flag conectado o GATT anterior no bastan.
- Duplicar Iniciar no reinicia el plazo. Parar durante la comprobación cancela. A45s sin confirmación se cancela sin empezar; resultados tardíos no arrancan una sesión cancelada. Un fallo BLE, desconexión o pérdida de condiciones seguras cancela también.
- Se mantienen subida10min, CH50/50/50, PV96/150/210 al15%, mantenimiento hasta Parar y retorno2min a programación actual. Perfil guardado intacto, sesión en RAM sin reanudar tras reinicio.
- Panel: título, una línea de estado, progreso durante subida/retorno y botones Iniciar/Parar. Sin párrafos explicativos ni valores técnicos en el panel. Información completa en Informe Maestro.

## Alcance preservado y pruebas

Motor de curvas, rampa de mantenimiento, worker/SDK BLE, colas, apagado, recuperación, tabla de particiones y todos los componentes de almacenamiento3.7 conservan sus hashes. Se cambia admisión de mantenimiento, diagnóstico de reposo y UI por petición explícita del usuario. El fixture histórico storage_frozen_lighting_sha256.json se conserva; maintenance_3_8_frozen_sha256.json fija los componentes fuera del alcance autorizado. No se cambia la espera de arranque30/180s ni el scan inicial5s en esta corrección.

Pruebas ejecutan handlers reales con reloj/radio simulados: GATT antiguo, una sola luz, flags sin GATT nuevo, inicio correcto tras ambas, duplicados, Parar,45s, resultados tardíos, errores y conflictos seguros, rollover y perfil intacto. Gate real de reposo valida126s/300s y todas las exclusiones. DOM verifica panel sencillo, estado checking/activa/retorno/offline y «En reposo» sin verde de comunicación reciente. Suite heredada y laboratorio LittleFS se mantienen.

## Comprobación física de la candidata

1. Instalar BIN solo aplicación por Centro→Actualizaciones. Debe conservar FS gen10/38PV/32CH, sin nueva migración/escritura de curvas, sin formato.
2. En reposo nocturno, tras90s desde OFF, ver «En reposo», Iniciar habilitado y flag online_recent=false si procede.
3. Pulsar Iniciar: «Comprobando luces…», después subida solo tras ambas confirmaciones GATT. Verificar visualmente rampa, meseta y retorno ya probados en3.7.
4. Parar durante comprobación: no iniciar después, incluso con resultados tardíos. Con pantalla realmente fuera de alcance: no iniciar al no confirmar ambas; no usar un flag conectado como prueba de control.
5. Informe Maestro: revisar preparing, verification_timeout_sec, eventos MANTENIMIENTO_COMPROBANDO/INICIO/CANCELADO y diagnósticos. Esta candidata requiere nueva validación física; el buen resultado de89 corresponde a3.7.
