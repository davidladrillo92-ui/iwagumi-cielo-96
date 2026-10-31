# CIELO — ficha de publicación (borrador)

Estado: candidata 0.2.1, sin publicar. Usar estos textos solo cuando las pruebas en hardware y firma estén cerradas. Nombre e identificador sujetos a disponibilidad y derechos del titular.

**Nombre:** CIELO · Luz de acuario

**Subtítulo:** Curvas, luz y mantenimiento

**Idioma principal:** español (España)

**Categoría propuesta:** Utilidades

**Palabras clave:** acuario,iluminación,curvas,mantenimiento,RGB,control,red local

## Descripción

Controla la iluminación de tu acuario desde el iPhone con CIELO y tu controlador ATOM compatible.

Consulta las consignas RGB y el estado de comunicación de Chihiros y Pez Verde en una interfaz sencilla. Revisa las curvas diarias, ajusta sus puntos e importa o exporta perfiles JSON. Guarda ambas curvas en una sola operación.

Activa la luz de mantenimiento para trabajar en el acuario con una subida gradual. Al terminar, detén el mantenimiento y el controlador volverá suavemente a la programación.

Desde Centro puedes preparar un informe de diagnóstico y compartirlo cuando lo necesites.

Incluye un modo demostración para explorar la app sin equipo. El control real requiere un ATOM con firmware CIELO compatible y conexión a la misma red local. Los valores mostrados son consignas del controlador: la comunicación Bluetooth no mide la luz física. El ATOM continúa ejecutando las curvas cuando cierras la app. La vigilancia de Telegram utiliza el vigilante externo del iMac; la app no sustituye ese servicio.

CIELO no incorpora publicidad, analítica ni registro de cuentas.

## Notas para App Review (borrador en inglés)

CIELO is a companion controller app for an independently configured ATOM running compatible CIELO firmware. It starts in a clearly labelled demonstration mode. No account is required.

In Demo mode: open Light to inspect simulated status and start/stop simulated maintenance; open Curves to inspect and edit points, import/export a JSON profile, and save to the demo session; open Centre to prepare and share a simulated report. Demo maintenance timing is simplified and no physical light is controlled.

Real operation: connect the iPhone to the same LAN as the ATOM. On Light, tap Connection, disable Demo mode, and enter the private IPv4 address of the controller. Local-network permission is required. The app uses HTTP only on the specified private LAN address. Review video and hardware access instructions must be supplied by the developer before submission.

Bluetooth control and autonomous schedules run on the ATOM, not on the iPhone. The iPhone app does not scan Bluetooth or monitor in the background. RGB and percentages are requested settings, not optical measurements. Telegram notifications are provided by a separate existing macOS observer.

## Datos que debe completar el titular

- Cuenta Apple Developer, Team ID, bundle ID definitivo y SKU.
- URLs públicas reales de soporte y privacidad, correo de soporte y titular/copyright.
- Precio y territorios, clasificación por edades y declaración comercial cuando corresponda.
- Declaraciones de App Privacy y exportación revisadas para el binario final.
- Capturas de la app real, sin información privada, para los tamaños exigidos por App Store Connect.
- Vídeo físico de iPhone y controlador: conexión, cambio de consigna, mantenimiento y retorno.
- Acceso o instrucciones completas para revisión del hardware. No incluir contraseñas privadas en estos archivos.

## Privacidad: contenido técnico para la página del titular

El cliente actual no integra SDK de publicidad, seguimiento ni analítica. Consulta el ATOM de la dirección privada elegida por el usuario. La dirección y el borrador se mantienen durante la sesión. La app no envía datos a un servidor propio ni crea cuentas.

La exportación de perfiles y la preparación de informes requieren una acción del usuario. Los informes preparados se guardan en el espacio temporal privado de la app; compartirlos mediante iOS puede enviarlos a la aplicación o destino elegido por el usuario. El usuario debe revisar el contenido antes de compartirlo. El informe depende de la versión del firmware del controlador.

Este apartado describe el comportamiento del código de la candidata 0.2.1. No es una política publicada ni sustituye los datos de contacto y responsabilidad que debe aportar el titular. Revisar de nuevo antes de enviar a Apple y tras añadir cualquier servicio externo.
