# CIELO iOS — candidata nativa0.2

Proyecto SwiftUI para iPhone/iPad, iOS17 o posterior. Base del firmware3.12 c2cb7fedcc0cc887f003f0c84bcac0a0b8748f39. Esta rama no modifica el firmware ni instala nada en el ATOM o el iMac. No es una app publicada ni un IPA firmado.

## Funciones implementadas

- Pantallas nativas Luz, Curvas y Centro; gráficos Swift Charts con puntos y editor de hora/RGB/brillo. Un borrador separado requiere Guardar para aplicar ambas luces. Importa y exporta el mismo JSON v2 y checksum FNV que la web; generación y boot protegen frente a cambios remotos. La edición/importación no envía órdenes.
- Mantenimiento usa los endpoints existentes y su confirmación explícita. La verificación y rampas de10/2min las ejecuta el ATOM. Sin reintentos automáticos de POST; tras fallo se conserva el borrador y se pide consultar el estado.
- Informe compartible solicitado por el usuario y exportación nativa. No se incluyen SDK de seguimiento, cuentas, anuncios ni Telegram en el cliente. La vigilancia continúa siendo externa.
- Modo demostración explícito, sin llamadas de red, con curva de referencia. Permite editar/exportar/guardar un perfil de demostración y alternar mantenimiento simulado; los tiempos físicos solo se prueban en hardware. Demo es el modo inicial para evitar acciones al abrir por primera vez.
- Conexión manual a IPv4 RFC1918. Permiso de red local, excepción ATS limitada a redes locales; redirecciones rechazadas. IP solo en memoria, no UserDefaults. Sesión efímera, sin cookies/caché de estado. Consultas6s en primer plano, frescura20s y estado invalidado al salir. La app no garantiza vigilancia en segundo plano ni control remoto por Internet.

## Compilación y prueba

Abrir `Cielo.xcodeproj`, esquema CIELO. `generate_project.py` lo reconstruye sin dependencias. CI registra versiones Xcode/SDK, prueba el core Swift y compila simulador e iPhone sin firma. No se puede instalar el paquete sin firma en un iPhone de usuario. La firma y distribución requieren identidad/provisionamiento del titular de Apple Developer. La compilación en GitHub evita depender del iMac Sierra, que conserva su función de vigilante.

El perfil demo es el último JSON preparado de37PV/44CH, checksumdd129bb7. Tests del core verifican el checksum, importación/exportación, generación, tiempos duplicados y dirección privada. El transporte HTTP simulado comprueba conflictos de generación, formularios de guardado conjunto, borradores conservados con timeout/409, ausencia de reintentos automáticos y órdenes de mantenimiento. Estos tests no sustituyen ensayos de las operaciones HTTP en un iPhone real.

## Pendientes antes de TestFlight/App Store

1. Titular y membresía Apple Developer, Team ID, bundle ID disponible, registro de la app en App Store Connect y certificados/profiles. No compartir contraseña o claves privadas en el chat. Configurar la firma mediante el flujo seguro del titular.
2. Prueba real iPhone: permiso LAN, pérdida de Wi-Fi, retorno del segundo plano, edición/import/export, fallo y confirmación de guardado, mantenimiento/retorno, 409 por otra edición y posterior informe. Comprobar solicitud HTTP local con ATS y firmware3.11/3.12. La app y el ATOM deben estar en la misma red.
3. Completar accesibilidad y pruebas de UI, simulador/iPad y texto grande; afinar el diseño para equivalencia visual con la web. Esta primera candidata no añade/elimina puntos, no instala OTA ni configura Wi-Fi desde la app; son ampliaciones posteriores, no funciones anunciables aún.
4. Publicar URLs reales de soporte y privacidad del titular, completar ficha en español, capturas reales y declaraciones de privacidad/exportación. El manifiesto describe el código actual sin SDK externos; revisar de nuevo si cambia.
5. Proporcionar instrucciones de demo y vídeo físico iPhone↔ATOM para App Review. Explicar dependencia del controlador CIELO y la ausencia de afiliación con fabricantes de luces. Confirmar derechos de nombre/recursos. Apple decide la aprobación; no se garantiza.
6. Crear archivo firmado con el Xcode/SDK exigido en la fecha de envío (consulta oficial del31/10/2026: mínimo Xcode26 y SDK iOS26 desde28/04/2026; volver a verificar al enviar); beta TestFlight primero, revisión App Store después. No publicar esta candidata sin cerrar los puntos anteriores.

## Referencias oficiales consultadas31/10/2026

- https://developer.apple.com/app-store/review/guidelines/ — funcionalidad mínima4.2 y aplicación completa2.1.
- https://developer.apple.com/help/app-review/before-submitting-for-review/complete-review — hardware/vídeo y datos de revisión.
- https://developer.apple.com/documentation/bundleresources/information-property-list/nsapptransportsecurity/nsallowslocalnetworking — HTTP local y direcciones IP.
- https://developer.apple.com/programs/ — membresía y distribución TestFlight/App Store,99USD/año o importe local aplicable.

La ficha de publicación está preparada en `FICHA_APP_STORE_BORRADOR.md`. Incluye textos de tienda y notas de revisión; el titular debe completar sus datos y publicar las URLs.

Actualización0.2: vista previa RGB por minuto, acciones antes de los puntos, protección de primer plano/conflictos, pruebas nativas de interfaz y guía de firma. Ver `PRUEBA_BETA_Y_FIRMA.md`. La apertura en simulador y los ensayos automáticos no sustituyen la prueba con el ATOM ni la firma real.
