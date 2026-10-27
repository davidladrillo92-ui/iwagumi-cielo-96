# RECOVERY3.11 — Curvas más sencillas

27/10/2026. Base: RECOVERY3.10, fuente05ba271807cace7fffe1f680516a41796d07b054, PR63. Cambio solicitado tras las capturas de las14:41: sustituir el recuadro HORA SIMULADA por las filas con RGB y porcentaje que estaban al final de la página. No se ha adjuntado un Informe Maestro nuevo con estas capturas.

## Diseño implementado

La gráfica, la hora seleccionada, el deslizador y las dos luces quedan reunidos en una tarjeta. Chihiros aparece primero y Pez Verde después, siguiendo el orden de Luz y Centro. Cada fila contiene su muestra de color, nombre, RGB, porcentaje y acceso al editor. Una sola hora controla las dos filas. El botón Ahora recupera la hora del ATOM.

Se retiran el recuadro técnico, las muestras de color anónimas, la hora repetida, la explicación larga permanente y el encabezado redundante24 HORAS. Los nombres RGB ya aparecen junto a cada luz. La indicación visible se limita a «Vista previa · no cambia las luces». Las escalas de RGB y la explicación de Comparar quedan en Más opciones.

Las horas del eje se sitúan inmediatamente debajo de la gráfica. La leyenda es breve y usa el mismo orden de las filas. Se eliminan las líneas azules auxiliares del canal B que aparecían sin leyenda en vistas individuales; los canales RGB siguen visibles en las filas y en el editor. No cambia la curva programada.

El botón Editar punto aparece cuando hay un punto seleccionado. Se corrige la selección inválida tras cambios del editor para que no habilite una acción sobre un punto inexistente. Los seis controles de edición forman una cuadrícula estable de dos filas; el estado sin cambios se muestra como texto discreto, y el aviso destacado se conserva cuando hay cambios pendientes.

## Comportamiento conservado

- La hora, la gráfica, sus filtros, el zoom y abrir un editor son acciones locales. No guardan ni envían órdenes a las pantallas.
- Las filas consultan la misma interpolación vAt del editor, verificada frente al motor C++ en1441 minutos. Chihiros muestra RGB porcentual y el mayor canal como porcentaje; Pez Verde conserva RGB0–255 y brillo independiente. La muestra de Pez Verde escala visualmente su RGB por el brillo, sin cambiar el RGB escrito en la fila.
- Son valores programados para la hora elegida, no medidas físicas. Cada muestra conserva una etiqueta accesible que explica su carácter calculado. El deslizador anuncia la hora mediante aria-valuetext. La gráfica mantiene navegación con flechas/Home/End y selección táctil de puntos.
- Guardar, importar/exportar, deshacer/rehacer, comparar y mantenimiento conservan sus puertas de seguridad y comportamiento. Guardar sigue siendo una acción explícita.

## Alcance y validación

main.cpp es idéntico a3.10 salvo el identificador de versión. Los dos módulos Wi-Fi, la tabla de particiones, motor, transporte BLE, recuperación, mantenimiento, almacenamiento y vigilante Sierra se conservan. No se cambian rampas ni esperas de arranque. La interfaz se vuelve a generar en src/cielo_ui.cpp.

La suite host mantiene la comparación de interpolación de1441 minutos y los hashes de conservación. Chromium añade comprobaciones a320/390/768px: tarjeta única con ambas luces a15:01, RGB/porcentajes coherentes, eje bajo la gráfica, ausencia de desbordamiento, límites00:00/23:00/24:00, filtros, selección táctil, apertura de ambas vías de edición y ninguna petición POST al consultar. Recorre además el resto de controles, guardado fallido, Wi-Fi y OTA con API simulada. Las capturas de prueba contienen una curva sintética, no telemetría nueva del ATOM.

La entrega identifica el resultado concreto de Actions, commit, artefacto y SHA256 del BIN después de compilarlos. Queda pendiente instalar la candidata y comprobar el diseño en Safari/iPhone con el perfil real, así como descargar el siguiente Informe Maestro. No se declara probada físicamente esta versión.
