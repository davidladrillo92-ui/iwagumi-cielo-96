# CIELO iOS 0.2.1 — validación para preparar la beta

Esta revisión cierra comprobaciones automáticas que faltaban en0.2. No cambia firmware, controlador BLE, particiones, curvas instaladas ni vigilante del iMac.

## Cambios

- Botones de mantenimiento con superficie mínima de44puntos y estado deshabilitado visible. Con los tamaños de texto de accesibilidad se colocan en vertical.
- Filas de luces y vista previa adaptadas a texto grande; gráfico con altura escalable y menos marcas horarias en ese tamaño. Símbolos decorativos excluidos de la lectura accesible.
- La demostración estática se actualiza al abrir, guardar o usar sus controles, sin reconstrucciones periódicas. Las consultas periódicas y su límite de frescura se mantienen para el ATOM real.
- Accesos directos de vista previa a00:00 y24:00 para elegir los extremos con precisión. El arrastre del deslizador se prueba como desplazamiento, sin asumir que la automatización nativa llega exactamente al minuto extremo en todos los dispositivos.
- Edición de horas limitada al intervalo entre vecinos. El punto final permanece a24:00; la Chihiros conserva sus canales finales a cero y Pez Verde conserva el brillo inicial/final a cero. Sigue existiendo la validación completa antes de guardar.
- Títulos dentro del contenido de cada sección, con tipografía semántica y rasgo de encabezado para VoiceOver, y contenedores adaptativos que conservan sus vistas al cambiar el tamaño de texto. La configuración de conexión se inicializa una vez al abrirla para mantener la edición durante las consultas de estado.
- Aviso de demostración breve y con fondo opaco, para evitar superponer texto sobre el contenido desplazado, también con tamaño máximo. VoiceOver conserva la descripción de que no controla las luces.
- Confirmaciones con identificadores independientes para probar diálogos y ventanas emergentes en iPhone/iPad.

## Comprobaciones incluidas

La CI ejecuta seis recorridos por familia en dos Mac independientes: navegación/vista previa/informe; edición y guardado en demo; IP pública rechazada; inicio/parada de mantenimiento en demo; auditoría de accesibilidad seleccionada; orientación horizontal y retorno del segundo plano.

Cada incidencia conserva pantalla, descripción y referencia al elemento; el recorrido continúa por las tres pestañas y todas las incidencias siguen contando como fallos.

La auditoría automática usa los tipos Dynamic Type, texto recortado y descripción suficiente de elementos de Apple sobre las vistas principales visibles. No sustituye una revisión manual con VoiceOver, no cubre todos los elementos fuera de pantalla ni certifica accesibilidad completa. Contraste, interacción manual y uso físico siguen pendientes.

Una ejecución separada fija el tamaño del sistema mediante `simctl ui content_size accessibility-extra-extra-extra-large`. Verifica el crecimiento real del título (>35pt de altura), disposición vertical de mantenimiento, vista previa y compartir informe; conserva capturas y resultado propios. No reemplaza ni oculta los fallos del auditor Dynamic Type. El tamaño se restaura al terminar.

Las fuentes y compilaciones sin firma de una candidata pueden adjuntarse para revisión aunque la auditoría falle: su manifiesto conserva el resultado fallido y el job termina en fallo. Estos paquetes no equivalen a una entrega validada.

La entrega conjunta solo se genera si las dos familias pasan sus seis pruebas sin fallos ni omisiones y la prueba adicional de tamaño máximo. Se cotejan los fuentes del ZIP con el checkout, la procedencia del mismo commit/ejecución y los hashes de los artefactos. `VERIFICACION_ENTREGA.json` conserva expresamente el estado sin firma, sin publicación y no listo para TestFlight.

## Pendientes que requieren al titular o el equipo físico

- Cuenta Apple Developer activa, Team ID y bundle ID definitivo; firma y validación de archivo en Xcode. La sintaxis del procedimiento de firma está comprobada, pero no se ha ejecutado una firma real.
- Sesión iPhone↔ATOM: permiso LAN, pérdida de red, guardado explícito, comprobación física de mantenimiento/retorno e informe. Los datos simulados no demuestran la luz física.
- Revisión manual de accesibilidad, privacidad/soporte públicos del titular, ficha y vídeo para revisión. La app candidata aún no añade/elimina puntos ni configura Wi-Fi/OTA.

El iMac Sierra continúa como vigilante; estas comprobaciones de iOS se realizan en los Mac de GitHub. Las instrucciones de firma y sesión física están en `PRUEBA_BETA_Y_FIRMA.md`.

Referencia oficial: https://developer.apple.com/documentation/accessibility/performing-accessibility-audits-for-your-app

Referencia de contenedores adaptativos: https://developer.apple.com/documentation/swiftui/anylayout
