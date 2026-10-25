# RC6 RECOVERY2 · presencia BLE y encendido Pez Verde

## Evidencia física y límites

El propietario instaló RECOVERY1 (informe 61/62, arranque 116). Observó Pez Verde sin luz; luego la desconectó de la corriente. El informe 62 del 25/10/2026 17:14:02 Madrid seguía indicando enlace conectado y CORRECTO con último éxito de hace 247 segundos. Los contadores no aumentaron desde el informe 61: no demuestra envíos exitosos sin alimentación. Los dos archivos 61 recibidos eran idénticos. No se publican informes privados ni datos de red.

SIMPLE1 liberaba los enlaces después de 90 segundos sin operaciones. RECOVERY1 suprimió esa liberación mientras existía control automático; ambas versiones reenviaban consignas sin cambios cada cinco minutos y admitían 650 segundos como éxito reciente. Por tanto, la regresión del ciclo de enlace es nueva; la ventana permisiva de diagnóstico era anterior. No se ha demostrado por qué el callback de desconexión no se reflejó en este caso.

## Corrección

- Reenvío de consigna cada 30 segundos, también en tramos planos; no cambia el intervalo configurado de la curva ni sus puntos.
- Reconciliación de clientes en la tarea BLE cada vuelta (espera normal de un segundo), invalidando característica, preludio y estado de encendido al perder enlace. HTTP sigue sin tocar la pila BLE. Flags compartidos atómicos; callback guarda hora de desconexión.
- Se solicita intervalo de conexión 30–50 ms y supervisión de 4 segundos a NimBLE. Es una solicitud de parámetros, no una promesa de detección física exacta a los cuatro segundos.
- Presencia reciente exige enlace declarado conectado, éxito de menos de 90 segundos y ausencia de errores consecutivos. Se aplica al estado principal y al diagnóstico profesional. Se publican conexión por dispositivo y límite de frescura.
- La interfaz muestra «BLE reciente», «Sin enlace BLE» o «Sin verificar»; nunca deduce luz física del éxito GATT. El estado conectado de la pila tampoco es una medición óptica.
- Pez Verde recibe POWER ON en cada consigna positiva, seguido de RGB y brillo, sin confiar en pvOn de una operación anterior. La prioridad OFF se vuelve a comprobar tras conexión y entre tramas. Brillo cero solo envía OFF. Es una corrección preventiva para apagados externos; no demuestra la causa de la falta de luz observada.
- Se mantienen NimBLE 2.3.6, conexión asíncrona 8 s, supervisor de emergencia 120 s, cesión a aplicaciones y OTA, perfiles NVS, códecs y particiones.

## Validación y ensayo físico pendiente

Pruebas host ejecutan reconciliación y callbacks reales contra clientes simulados desconectados, frescura del informe 62, reloj con rollover, reenvío de encendido con estado antiguo, OFF durante conexión/tramas y brillo cero. El test de interfaz ejecuta el JavaScript real, incluyendo un recent=true antiguo contradicho por connected=false. La suite completa y la compilación ESP32-S3 deben pasar antes de entregar el binario.

Validación física pendiente: comprobar luz tras instalar; cortar alimentación PV manteniendo ATOM activo; confirmar que deja de presentarse como BLE reciente y registra fallos/reconexión; restaurar alimentación y comprobar recuperación luminosa. Repetir apagado externo, curva plana, medianoche y arranque seguro. Una prueba host o Actions no valida el hardware ni la emisión de luz. La versión Sierra no cambia en esta corrección de firmware.
