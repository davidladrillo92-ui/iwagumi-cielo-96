# CIELO 0.2 — pruebas y preparación de TestFlight

Esta candidata requiere iOS17+. Mantiene el firmware instalado, la autonomía del ATOM y el vigilante de Sierra. La compilación CI es sin firma; no puede instalarse directamente en un iPhone de usuario.

## Hecho en esta revisión

- Vista previa por minuto con tarjetas Chihiros/Pez Verde: RGB, porcentaje y color calculado. No envía órdenes. Usa interpolación entera y redondeo del motor existente `cielo::at`, incluyendo descensos y extremos; no modifica puntos ni reproduce visualmente la luz física. La vista previa no representa el modo experimental subminuto de15s.
- Guardar/importar/exportar quedan antes de la lista de puntos, que se despliega cuando hace falta. No exige recorrer decenas de puntos para guardar. La edición queda bloqueada mientras hay una operación en curso.
- Guardar se deshabilita también ante cambio de arranque/generación. El borrador se conserva y puede exportarse; para cargar la versión remota se usa Descartar borrador.
- La app invalida la comunicación al salir de primer plano e ignora respuestas de consultas anteriores. No consulta ni habilita órdenes en segundo plano. El mantenimiento mantiene las comprobaciones del ATOM.
- Un409 de mantenimiento muestra el motivo recibido del controlador; no se confunde con un conflicto de curvas. La IP inválida se corrige en la propia pantalla de Conexión.
- XCUITest nativo para navegación/vista previa/informe, editar y guardar en demo, iniciar/parar mantenimiento en demo y rechazar una dirección pública. Adjunta capturas por prueba. El simulador iPad se abre y se captura por separado; esto no equivale a una prueba completa de UI en iPad.

## Firma preparada, sin cuenta ni claves incluidas

El titular debe registrar el identificador definitivo en Apple Developer, crear la app en App Store Connect y configurar su equipo/firma en un Mac con Xcode moderno. El bundle provisional `es.iwagumi.cielo` no está reservado ni su disponibilidad confirmada.

`archive_beta.sh --help` describe los tres parámetros. Una vez configurados equipo y certificados en Xcode:

```bash
CIELO_TEAM_ID=ABCDEFGHIJ CIELO_BUNDLE_ID=es.tuempresa.cielo CIELO_BUILD_NUMBER=3 bash ios/archive_beta.sh
```

Los valores del ejemplo son ficticios. El script valida los parámetros, crea un `.xcarchive` firmado y comprueba su firma. No carga credenciales del chat, no genera activos de cuenta automáticamente, no sube el binario ni publica la app. La firma real queda pendiente hasta disponer de la cuenta y de sus activos de firma; la CI solo comprueba la sintaxis y la compilación sin firma.

Abrir el archivo en Xcode Organizer, ejecutar Validate App y elegir distribución TestFlight. Completar App Store Connect: ficha, soporte/privacidad públicos, declaraciones aplicables, vídeo del hardware y datos de revisión. La subida/procesamiento no son una aprobación de App Review.

## Sesión real con iPhone y ATOM — todavía pendiente

1. Antes de conectar: guardar/exportar el perfil vigente. Confirmar que el ATOM funciona con la web y anotar firmware/generación. La prueba no necesita instalar otro firmware.
2. Instalar la beta firmada; abrir primero Demo y revisar Luz/Curvas/Centro. En Conexión, desactivar Demo y poner la IP privada real del ATOM; permitir red local. Deben aparecer firmware/arranque/valores recientes. No identificar acceso al ATOM con comprobación óptica de las luces.
3. Sin modificar curvas: cambiar la hora de vista previa. Verificar físicamente que las luces continúan su programación; solo cambia la representación en el iPhone. Volver a medianoche y comprobar ambos finales apagados en la vista previa.
4. Con el equipo supervisado: editar un punto interior de una copia, guardar explícitamente y comparar el perfil leído/exportado con la web. Restaurar la copia original si el cambio era de prueba. Comprobar que la forma horaria y los finales se conservan.
5. Mantener un borrador abierto y guardar otra versión desde la web: el botón de guardado de la app debe quedar deshabilitado después de consultar; exportar el borrador si se quiere conservar. Descartarlo carga el perfil remoto. No forzar la generación para sobrescribir.
6. Sin órdenes pendientes, quitar Wi-Fi/cerrar a segundo plano; al regresar, no debe indicar datos confirmados hasta obtener una respuesta reciente. Recuperar la red y revisar estado. La app no promete Telegram ni vigilancia en segundo plano.
7. Iniciar mantenimiento con las luces físicamente presentes: comprobar la preparación, subida de10min y estabilidad; pulsar Parar y verificar retorno de2min a la curva. Revisar el informe compartido. No usar una prueba de desconexión eléctrica durante un apagado como primera validación.
8. Revisar texto grande, VoiceOver, accesibilidad de controles, iPhone/iPad y orientaciones. Las capturas automáticas no prueban por sí solas accesibilidad ni funcionamiento físico.

## Evidencia necesaria antes de publicación

Conservar el manifiesto de compilación y resultados CI; registrar modelo/iOS, firmware/arranque/generación y resultado de cada paso real. No publicar informes privados ni credenciales. Hasta completar firma, sesión real, ficha y revisión, el estado es candidata sin publicar.

Referencias oficiales consultadas:
- https://developer.apple.com/documentation/xctest/adding-attachments-to-tests-activities-and-issues
- https://developer.apple.com/documentation/xcode/adding-tests-to-your-xcode-project
- https://developer.apple.com/help/app-store-connect/manage-builds/upload-builds
- https://developer.apple.com/documentation/xcode/distributing-your-app-for-beta-testing-and-releases
