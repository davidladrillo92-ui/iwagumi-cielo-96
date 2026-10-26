# RECOVERY3.6 — luz de mantenimiento

Base: RECOVERY3.5.2, commit 7d925e83c55fa26e3c153eb6144b9ec9e24a6e22, PR55.
Informe80 (26/10/2026 19:29 Europe/Madrid): boot131,4666s, generación9,
38PV/31CH; ambas copias NVS válidas (9 y8), guardado verificado.77 operaciones
PV y76 CH correctas,0 errores BLE y0 desconexiones inesperadas. Sigue siendo
el mismo arranque: no prueba persistencia tras reiniciar ni emisión óptica.
SPIFFS no montable y no vacía permanece intacta. No publicar el informe privado.

## Uso

Tarjeta Luz de mantenimiento inmediatamente debajo de las dos pantallas,
con progreso y botones Iniciar mantenimiento / Parar mantenimiento.

- Chihiros:50/50/50. Pez Verde:azul96/150/210, brillo hardware15%.
- Inicio:10min desde la consigna conocida actual; interpolación smoothstep.
- Se mantiene hasta pulsar Parar. Cerrar Safari no interrumpe la sesión.
- Parar:2min hacia el programa de la hora actual. Por la mañana termina
  apagado; en horario diurno retorna al valor programado, que puede ser
  mayor o menor que50/50/50.
- Pulsaciones repetidas/reintentos HTTP no reinician ninguna rampa.
- Reiniciar ATOM cancela la sesión RAM; el arranque habitual recupera el
  programa. No se promete un apagado físico instantáneo al reiniciar.

Las consignas propuestas requieren observación visual; no son intensidad
calibrada ni verificación óptica. Los enteros del protocolo producen pequeños
pasos, evaluados aproximadamente cada10s. El azul puede ajustarse después.

## Integración y seguridad

Máquina Idle/Rising/Holding/Returning en cielo_worklight.h, sin heap, flash,
radio ni esperas bloqueantes. desiredPv/desiredCh incorporan la salida temporal
sobre programPv/programCh. Usa el mismo planificador serial y transporte
Bluedroid persistente; no crea trabajadores BLE ni desconecta entre envíos.

POST /api/work-light/start requiere confirm=INICIAR_MANTENIMIENTO.
POST /api/work-light/stop es idempotente. El estado work_light se incluye
en /api/status e Informe Maestro: fase, tiempos, objetivos, consignas calculadas,
disponibilidad, cancelación y ausencia de verificación óptica. Es independiente
de /api/maintenance, que sigue siendo inventario de memoria/USB.

El inicio exige arranque sano, reloj/perfil/BLE válidos, CIELO controlando ambas
luces, comunicación reciente y ausencia de OTA, rescate, interlock pendiente,
retorno Chihiros, pruebas manuales/laboratorio/cadencia o recuperación BLE.
La UI muestra el motivo. Una orden en curso puede pedir esperar unos segundos.
No guarda ajustes temporales ni modifica puntos/generación en NVS.

Durante la sesión, OTA, cesión BLE y guardado de curvas piden terminar
mantenimiento primero. Solicitar programación, pausa o My Chihiros lo cancela.
Un error BLE, desconexión inesperada, reloj inválido o pérdida de condiciones
cancela la sesión, retira pendientes y devuelve decisiones a programa/seguridad.
Una llamada ya dentro del SDK no se cancela artificialmente; se conservan los
plazos y mecanismos de recuperación previos. Sin radio no se garantiza apagar
físicamente. El inicio nocturno requiere OFF GATT previo confirmado; el modo
temporal autorizado sustituye la consigna cero, y al regresar a cero se exige
de nuevo POWER OFF. Se conserva el interlock del trabajador.

El perfil guardado y la recuperación NVS3.5.2 permanecen sin cambios.
El observador Sierra4.0.1 no se modifica; CI conserva Python3.9.

## Diagnóstico del informe80

922303 en diag_writes_skipped_low_space contaba comprobaciones del loop,
no fallos de iluminación. Una omisión actualiza ahora el temporizador de5min
conservando journalDirty. Evita consultas NVS y crecimiento del contador en
cada vuelta. La prioridad del registro de reinicios no cambia.

## Pruebas y entrega

Host: rampa completa, mantener, parada anticipada/repetida, programa cambiante,
noche/día, cancelación, arranque RAM y rollover. Se ejecutan los handlers reales
con reloj/radio/servidor simulados: condiciones de admisión, cola, JSON, prioridad
OFF y perfil byte a byte intacto. UI real mediante Node, DOM estricto y rutas.
Handler real de desconexión. Journal real con100000 pasos simulados, sin escribir
en memoria cuando falta espacio. Suite anterior NVS/SPIFFS/BLE/OTA y Sierra.

La suite local pasó antes de interrumpirse el entorno; se recuperó el código
desde la fuente base y se reconstruyeron los cambios para repetir todas las
pruebas y compilar ESP32-S3 en Actions. El resultado definitivo y la identidad
del BIN se documentan en el PR de entrega. No hay validación física de3.6.

Antes de instalar: exportar perfil activo y conservar BIN3.5.2 funcional.
Después: comprobar generación9/38PV/31CH, iniciar una vez supervisando ambas
pantallas, probar Parar a mitad de subida y revisar retorno al horario; descargar
Informe Maestro. No es necesario volver a guardar la V5 para usar mantenimiento.
