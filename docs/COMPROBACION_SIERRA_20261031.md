# Verificación de instalación — continuación de la auditoría3.12

Paquete independiente del firmware y del vigilante. No sustituye ni instala ninguno. Parte de la fuente3.12 `c2cb7fedcc0cc887f003f0c84bcac0a0b8748f39` y resuelve la recogida segura de evidencia que falta para la siguiente validación en casa.

`COMPROBAR_CIELO.command` utiliza el Python3.9 existente en Sierra y solicita la IP local del ATOM. Hace tres GET acotados: status, Maestro compartible, status; compara boot y generación para identificar un reinicio o un cambio de perfil entre muestras. No reintenta una escritura ni ejecuta POST. Solo admite HTTP a IPv4 privadas RFC1918, rechaza redirecciones y credenciales en URL y no usa proxies del entorno. Cada lectura tiene8s de plazo y128KiB de límite.

La salida guarda únicamente datos permitidos: versión reconocida, boot, actividad, generación, checksum y resultados PASS/FAIL/UNKNOWN/NOT_TESTED/PENDING. No guarda las respuestas originales ni mensajes de excepciones. Identifica la fuente del vigilante mediante SHA256 y consulta su registro launchd; no ejecuta ni importa código instalado, no lee configuración/tokens, logs ni SQLite. Archivo coincidente o servicio registrado no prueban proceso activo, cobertura reciente ni entrega de Telegram: esos resultados se dejan explícitamente sin verificar.

La identidad de referencia se genera sobre la fuente4.0.1-SIERRA conservada en el repositorio. Si difiere, se informa sin actualizarla, borrarla o detenerla. El verificador nunca envía un mensaje al bot. El ensayo real de Telegram debe hacerse mediante una acción expresa posterior del propietario.

Opcionalmente se admite un Informe Maestro local y un perfil exportado. Se validan orden, límites, apagado, esquema y checksum del perfil; solo se confirma identidad cuando coincide con la huella que proporciona3.12. Un informe3.11 queda pendiente para esa comprobación, sin recomendar reinstalar ni modificar curvas. Las comparaciones indican el estado de esa muestra; no certifican por sí solas el almacenamiento tras un corte eléctrico.

Los resultados se crean con permisos0600, nombre exclusivo y carpeta privada. La única escritura es el nuevo JSON dentro de la carpeta de resultados del paquete. Pruebas con entradas incompletas/malformadas, redirección, respuestas grandes, direcciones públicas, cambio de snapshot, perfiles corruptos y señuelos secretos. CI valida Python3.9 y3.12 con servicios y red simulados; no certifica la ejecución en el iMac real.

Uso: descomprimir, abrir COMPROBAR_CIELO.command, pulsar Intro para192.168.0.25 y enviar el JSON de RESULTADOS. Para comparar el perfil desde Terminal: `python3.9 cielo_verify_sierra.py --report INFORME.json --profile PERFIL.json`.
