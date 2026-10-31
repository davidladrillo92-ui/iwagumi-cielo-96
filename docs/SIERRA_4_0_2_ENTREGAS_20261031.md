# Sierra4.0.2 — preservar avisos pendientes

Continuación de la auditoría de diagnóstico. Base: firmware3.12, commitc2cb7fedcc0cc887f003f0c84bcac0a0b8748f39, con vigilante4.0.1-SIERRA. No se ha accedido al iMac ni enviado Telegram real. El BIN del ATOM no cambia.

## Fallo demostrado en el código anterior

`issue()` encolaba alerta y recuperación con la misma clave, por ejemplo `pv`. `outbox()` usaba INSERT OR REPLACE. Si Telegram no había recibido la alerta antes de que la condición se recuperase, la recuperación sustituía el aviso pendiente. El historial de eventos sí podía contener ambas entradas, aunque la cola conservase solo una. No se afirma que explique retrospectivamente ningún mensaje concreto del propietario sin revisar su instalación e historial reales.

`check_daily()` usaba siempre la clave `daily`: un resumen del día siguiente podía sustituir uno aún pendiente. Reencolar la misma identidad también reiniciaba intentos y vencimiento del reintento. Tres regresiones fallan contra4.0.1: alerta+recuperación sin entrega intermedia, dos resúmenes diarios y duplicación durante backoff.

## Corrección

- Claves de entrega independientes para alerta, recordatorio y recuperación, identificadas por condición/tipo/fecha de detección. El estado persistido existente sigue controlando la deduplicación y los umbrales.
- Clave `daily:AAAA-MM-DD` para cada día realmente resumido. Se conserva una sola creación por día mediante `last_daily`.
- INSERT OR IGNORE conserva el mensaje, intentos y vencimiento ya existentes al repetir exactamente la misma identidad. Los registros antiguos mantienen su clave y se siguen entregando; no se borra ni reescribe la cola para migrarla.
- Hora de detección en los mensajes de condiciones. La fecha es útil si llegan con retraso o fuera de orden por backoff. No se promete orden cronológico de recepción: cada mensaje respeta su próximo intento.

No se modifican token, chat, SSID, certificados, formato SQLite ni umbrales. Se conserva un envío por ciclo, backoff y tratamiento429. La actualización existente conserva configuración/DB, hace copia y permite volver atrás. Los avisos ya sustituidos por el código antiguo no se pueden reconstruir automáticamente. Una instalación distinta de la ruta/formato conocido se sigue rechazando antes de detenerla.

## Comprobaciones y límites

Cuatro pruebas nuevas ejercitan cola SQLite real, reinicio del proceso, recuperación antes de entrega, dos fechas, reencolado y error de envío sin fuga de secretos. Se conservan las once pruebas del instalador Sierra: backup, actualización/rollback, primer reinicio entre consultas, certificados, formato de historial, prueba Telegram simulada y gramática Python3.9. CI ejecuta ambas suites en3.9 y3.12 y empaqueta sin configuración, DB o logs.

La aceptación por Telegram y el borrado local no forman una transacción atómica. Si Telegram acepta y el proceso cae antes de confirmar SQLite, podría repetirse un mensaje. Se ofrece persistencia y deduplicación local; no garantía de entrega exactamente una vez ni funcionamiento con iMac apagado. Tampoco se simula disponibilidad óptica.

Antes de actualizar en casa: ejecutar el comprobador del PR66 para identificar la instalación, conservar su resultado y descargar el informe del ATOM. Actualizar solo mediante ACTUALIZAR.command sobre la instalación identificada. Después verificar versión4.0.2 y muestra reciente con CONSULTAR_ESTADO.command. El comando ENVIAR_PRUEBA_TELEGRAM.command requiere que el propietario lo ejecute expresamente para enviar el mensaje real; PROBAR_TELEGRAM.command solo diagnostica HTTPS. No se ejecutan estos comandos desde este desarrollo.
