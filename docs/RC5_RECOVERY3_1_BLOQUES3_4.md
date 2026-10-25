# RECOVERY3.1 — Bloques 3 y 4: Pez Verde diurna y diagnóstico BLE

## Base revisada
En RECOVERY3 el arranque exige OFF GATT de Pez Verde incluso durante el día. La reanudación automática conjunta de ambas pantallas espera además que termine el retorno de Chihiros. Esto puede alargar un apagón visible. El OFF nocturno debe prevalecer sobre cualquier optimización.

## Bloque 3: vía rápida diurna conservadora
- Se mantiene OFF inicial y su GATT aceptado: no se omite el interlock cuando la hora aún es desconocida.
- Tras OFF GATT, hora válida, perfil válido, curva actual con brillo mayor que cero, arranque saludable, worker vivo, BLE operativo y sin OTA/rescate, se permite reanudar Pez Verde sin esperar la recuperación de Chihiros.
- Se cancela la consigna antigua y se vuelve a calcular la actual; no se reproduce RGB obsoleto.
- El retorno de Chihiros se pospone como máximo 20 segundos para dar prioridad a la primera orden PV. Después se libera la prioridad aunque PV siga fallando.
- El OFF de la curva a brillo cero conserva prioridad. El envío GATT no es verificación óptica.
- Se registran intentos, GATT aceptados, estado y tiempo desde inicio.
- **No se elimina el apagón causado por el OFF obligatorio de arranque.** Se intenta acortar la recuperación posterior. Para evitar completamente ese OFF haría falta un diseño de estado persistente y garantías de hora/curva válidas antes de enviar luz; no se implementa aquí.

## Bloque 4: trazas y simulaciones
- Ring RAM de las 16 últimas transiciones de fase del worker, con ordinal, milisegundos desde arranque, fase y dispositivo; sin MAC ni contraseñas. No es persistente tras reinicio.
- `bluetooth.phase_trace` y `bluetooth.pv_day_recovery` en el Informe Maestro.
- Tests host deterministas: hora desconocida, noche, OFF no confirmado, recuperación diurna, retorno CH acotado, callback tardío simulado, conexión pendiente indefinida, worker bloqueado, OFF nocturno, clasificación GATT.
- **Los tests no simulan el SDK Bluedroid real** ni garantizan el comportamiento físico. Los callbacks REG/OPEN/SEARCH reales requieren instrumentación adicional y pruebas sobre ATOM antes de modificar timeouts del SDK.
- No hay inyección de fallos en firmware de producción ni botones peligrosos en la UI.

## Criterios de aceptación antes de instalar
1. CI host y PlatformIO compilados, sin regresiones.
2. Reinicio a plena luz: registrar OFF GATT, tiempo hasta nueva orden PV y comprobar visualmente el retorno.
3. Reinicio en horario OFF: nunca enviar ON/RGB por error, incluso con reloj inicialmente inválido.
4. CH desconectada o callback tardío: PV debe seguir funcionando y no quedar bloqueada por retorno CH.
5. Reinicio durante OTA y fallo de NVS: no permitir la vía rápida.
6. Comprobar que las curvas V5 no cambian; verificar LED y Telegram.
7. Informe Maestro válido y sin desbordamientos, memoria y stack estables durante 24-72 h.

**Estado:** candidato experimental; no fusionado ni instalado. No declarar la luz ópticamente verificada por una respuesta GATT.
