# RC5 SIMPLE1: interfaz sencilla y control automático

Versión candidata `11.1.0-RC5-B1-B9-SIMPLE1`. Se desarrolla sobre LAN1, instalada y confirmada por el Informe Maestro privado del usuario, arranque 107, generado el 22/10/2026 a las 01:56:52 de Madrid. SIMPLE1 no está instalada ni validada físicamente. No instalar durante la prueba nocturna en curso; revisar el informe posterior antes de otro ensayo supervisado.

## Análisis del primer informe de LAN1

- Uptime 723 segundos (12 min 3 s); `boot_marked_healthy=true`, `postboot_candidate_pending=false` y evento `OTA_POSTARRANQUE_OK` a los 195 s. En esta muestra no ocurrió el retorno de diez minutos.
- V5 generación 8: 34 puntos Pez Verde y 31 Chihiros, sin recuperación de valores predeterminados. Se preserva el paso lunar directo de 2/2/2 a 1/1/2 a las 23:00.
- Pez Verde: tres operaciones, dos finalizaciones GATT correctas y un `BLE_CONNECT_FAILED`. El fallo se registra a los 535,584 s; el reintento inicia a los 537,584 s y completa el envío POWER OFF a los 545,114 s. Esto prueba recuperación de la comunicación en esa ocasión, no luz física medida.
- Chihiros: dos operaciones correctas, cero errores. Ninguna prueba óptica de retorno 0x03 queda demostrada por este informe.
- Descubrimiento BLE: PV −89 dBm, CH −85 dBm. Son señales débiles, compatibles con la distancia/posición descrita por el usuario; el JSON no permite demostrar una causa exclusiva. Wifi −69 dBm, cero desconexiones. RSSI wifi y BLE son métricas distintas.
- Trabajador BLE en espera, heartbeat de 169 ms, sin operación en curso ni cola pendiente. No reproduce el atasco de 22 minutos del informe 41.
- RAM libre 108572 B, mínimo 64212 B, mayor bloque 77812 B; loopTask mínimo libre 11404 de 16384 B. No hay un fallo de memoria TLS en esta muestra; HTTPS no se inicia.
- Los cuatro avisos de más de 3,5 s incluyen operaciones que finalmente terminan bien. No son cuatro desconexiones. Las 31 incidencias persistentes y los reinicios anteriores pertenecen al historial; no se convierten en alarmas actuales por su mera existencia. El contador conserva su significado técnico en el informe.

## Qué se conserva y qué se retira de la interfaz

| Zona | Resultado |
|---|---|
| Luz | Paisaje, estado del ATOM y dos tarjetas con nombre, estado de comunicación, porcentaje y RGB breve. Columnas acotadas y altura flexible para evitar solapamientos. |
| Curvas | Gráfica, simulación, importar JSON, editar puntos, deshacer/rehacer, guardar y exportar. Importar no envía órdenes hasta guardar. |
| Centro | Estado global breve y estado por pantalla; un solo botón «Descargar informe», además del apartado Actualizaciones. |
| Ajustes | Wifi y hora; servicios opcionales, mantenimiento y pausa/cesión/reanudación dentro de «Más opciones». |
| Retirado del uso diario | Insignia Local, bloque Acceso local, cuatro botones de informes, tablas de RAM y tiempos, notas GATT en portada, prueba de 15 s/20 min, activación Natural V4 y comprobación guiada. |
| Diagnóstico técnico | Permanece completo en el Informe Maestro JSON: errores, eventos, señales, memoria, tiempos, perfiles, historial y recuperación. |

«Disponible» significa que hay una respuesta Bluetooth reciente. No afirma conexión permanente ni lectura óptica. Si se pierde comunicación con el ATOM se muestra «Sin comunicación» y porcentaje desconocido. Los reintentos activos muestran «Reconectando…»; los errores recuperados quedan en el JSON sin llenar de alertas la portada.

## Autonomía y Bluetooth

- Cuando no existe una preferencia previa, el control continuo se activa por defecto y se persiste. La búsqueda de nombres y las conexiones se hacen automáticamente desde el trabajador BLE. No hace falta escanear ni pulsar un botón de activación.
- Se conservan las comprobaciones de arranque, reloj, perfil, coexistencia y recuperación previa: no se envía una curva antes de que el estado permita hacerlo.
- Una pausa explícita guardada como `v5_always=false` se respeta. Reanudar solo hace falta si el propietario había pausado voluntariamente.
- Cadencia normal de comprobación: 30 s. Los colores usan la interpolación por minuto existente, sin alterar V5. No se fuerza un nuevo envío si el valor no cambia; el heartbeat y el apagado prioritario siguen funcionando. Se retira el selector experimental de la interfaz, no se inventan pasos de medio minuto en la curva.
- Los enlaces dejan de soltarse a los 5 s: se reutilizan entre cambios y se liberan tras 90 s sin actividad. La cesión explícita a My Chihiros y la preparación OTA siguen liberándolos inmediatamente. Esta política reduce reconexiones, pero necesita ensayo físico con ambas pantallas y no aumenta la potencia de radio.
- Pez Verde conserva sus reintentos, cancelación de RGB obsoleto, obligación de POWER OFF y recuperación tras atasco.
- Chihiros añade recuperación automática después del retorno por errores: espera inicial de 30 s, después 60/120/240 s hasta un máximo de 300 s si sigue fallando. Un envío GATT correcto restablece la espera inicial. Siempre envía la consigna actual, no reproduce colores antiguos.
- La recuperación de Chihiros queda inhibida durante OTA, rescate, pruebas manuales, retorno 0x03 pendiente, colas ocupadas, reloj/perfil/radio sin salud y pausa explícita. NVS debe aceptar la marca de toma de control antes de reanudar. No bloquea ni reactiva Pez Verde al recuperar únicamente Chihiros.
- El supervisor de atasco real del trabajador, con límite de reinicios, sigue integrado. No se elimina una tarea dentro de una llamada BLE ni se reinicia la pila desde HTTP.

## Ensayo siguiente

Mantener LAN1 esta noche; el registro posterior permitirá evaluar el comportamiento nocturno real. Para probar SIMPLE1, colocar el ATOM cerca de las pantallas, con alimentación estable y con supervisión. No prometer conexión infalible con señal débil.

Guardar el BIN `IWAGUMI_CIELO_RC5_SIMPLE1.bin` en el iPhone y cargarlo desde Actualizaciones de LAN1, sin clave ni permiso físico. Después abrir `http://192.168.0.25/`, comprobar SIMPLE1 y descargar un informe tras cuatro minutos y otro superados doce minutos. Comprobar las tarjetas a ancho de iPhone, importación sin guardar y control automático sin pulsaciones. Para probar pausa y reanudación, usar exclusivamente los controles excepcionales de Ajustes.

Quedan pendientes pruebas físicas de pérdida/recuperación de ambas pantallas, apagado nocturno de PV, retorno real My Chihiros, OTA desde esta versión, rollback y estabilidad prolongada. La protección eléctrica independiente de PV conserva su utilidad antes de ausentarse de casa.

Fuentes, pruebas y documentación se guardan en GitHub PR #34. CI empaqueta BIN, fuentes completas, manifiesto y símbolos; esta revisión no accede ni escribe en el ATOM.
