# Auditoría de interfaz y sistema — RECOVERY3.9

27/10/2026. Base: RECOVERY3.8.1, fuente3295b88af8077dae7f962fedadb4aea46c3a10b4. Repositorio davidladrillo92-ui/iwagumi-cielo-96. Solicitud: unificar ambas luces y las vistas Luz/Centro, revisar JavaScript, botones y proceso interno, y simplificar la experiencia.

## Conclusión y evidencia

Las capturas12:19 confirman una diferencia de presentación: Chihiros a0% se llama Conectado y Pez Verde a0% Conectado · en reposo; Centro cambia además el orden de las luces. La causa es mezclar estado de transporte con actividad luminosa y calcular cada representación por separado. Luz consulta status cada6s y Centro otro informe cada15s; podían describir momentos diferentes.

Informe90:3.8 instalada, boot139 saludable, PV15/15 y CH71/71 GATT, sin errores ni bloqueos; FS gen10/38PV/32CH, sin reescribir curvas. La captura posterior acredita la apariencia3.8.1, no sustituye un nuevo informe técnico. No hay acceso directo desde este trabajo al ATOM, al iMac o a Telegram.

Se entrega una candidata3.9 con correcciones de interfaz, concurrencia HTTP y diagnóstico de lectura. No se ha cambiado el motor de iluminación, el transporte BLE, las rampas, la verificación previa a mantenimiento ni el almacenamiento que ya superó migración/guardado/reinicio/no-op.

## Hallazgos corregidos

| ID | Prioridad | Hallazgo y corrección |
|---|---|---|
| U01 | P1 | Vocabulario distinto entre luces a0%. Una función pura lampPresentation aplica la misma regla: consigna cero confirmada→Conectado · en reposo/luna; consigna no cero confirmada→Conectado/check. No se usa el flag connected como prueba óptica. |
| U02 | P1 | Luz y Centro usan fórmulas y consultas independientes. Ambas filas reciben ahora el mismo objeto y la misma instantánea status. Mismos textos, orden Chihiros/Pez Verde y mismos cambios al perder conexión. |
| U03 | P1 | Centro podía mantener Todo en orden tras perder el navegador la comunicación. La caducidad local20s se revisa cada segundo sin consultas nuevas ni dibujo del gráfico; muestra Sin comunicación y deja de ofrecer órdenes. |
| U04 | P1 | GET de estado superpuestos o iniciados antes de un POST podían sobrescribir datos nuevos. GET único en curso, revisión de escritura y descarte/relectura de una respuesta anterior. Los cambios no guardados del editor se conservan. |
| U05 | P1 | Faltaban bloqueos de doble clic en varias acciones. Una petición POST por ruta en curso; botones asíncronos con estado ocupado y liberación incluso tras fallo. No hay reintento automático de escrituras. |
| U06 | P1 | OTA se bloqueaba después del preflight: dos pulsaciones podían superar esa ventana. Bloqueo desde el principio hasta el resultado; nuevo arranque saludable observado por boot_id antes de habilitar otra instalación aceptada. |
| U07 | P1 | Comprobación/preparación OTA sin límite de espera HTTP. API con timeout5s por solicitud, guardado15s, descarga15s y upload120s; se conserva límite de preparación y cancelación/restauración del firmware. La UI no da por instalado lo meramente transferido. |
| U08 | P1 | Guardar volvía a habilitarse aunque ya no hubiera cambios. Se recalcula desde dirty/guardado/conexión/mantenimiento/OTA; mensaje breve Curvas guardadas o Sin cambios que guardar. |
| U09 | P1 | Descensos JavaScript redondeaban mitades de forma diferente al motor C++. vAt reproduce el redondeo del motor; a24:00 conserva el RGB final almacenado, con su brillo cero. Comparación cruzada del motor real en1441minutos para ambas luces. Solo cambia la vista previa. |
| U10 | P2 | Títulos de amanecer/descanso dependían de horas de fábrica. Se derivan del perfil; a cero se muestra En reposo, también por la mañana. El mantenimiento conserva su estado propio. |
| U11 | P1 | Una simulación o edición local podía colorear la vista real. La simulación queda en Curvas; al ir a Luz/Centro se usa la consigna confirmada y el perfil recibido, separados del editor pendiente. |
| U12 | P2 |33 funciones JavaScript antiguas sin acceso desde controles vigentes, algunas con rutas retiradas y mensajes de laboratorio. Eliminadas; se sustituyen además tres funciones de diagnóstico duplicado por el modelo común. Los22 prefijos de API restantes tienen manejador en firmware. |
| U13 | P1 | Cerrar Wi-Fi durante un escaneo dejaba callbacks sobre controles que ya no existían. Se capturan sus nodos y se comprueba que el mismo panel siga abierto. Un escaneo agotado no se presenta como cero redes encontradas. SSID insertado como texto/opción, no HTML ejecutable. |
| U14 | P2 | Mantenimiento era también el nombre de una consulta de memoria. La opción técnica pasa a Diagnóstico de memoria, dentro de Más opciones. El panel principal conserva solo controles de luz. |
| U15 | P2 | Lecturas en pestañas ocultas y consultas voluminosas redundantes. status sigue6s cuando visible; se refresca al volver. Informe Maestro solo al descargar. Centro usa la salud resumida de status. |
| U16 | P1 | Botones de guardar/actualizar/descargar no siempre reflejaban conflictos ya conocidos. Se deshabilitan al faltar datos actuales y durante operaciones incompatibles, además de conservar los rechazos del servidor. |

La indicación Conectado · en reposo significa enlace disponible con última orden de cero aceptada o apagado confirmado dentro de su intervalo programado. No significa sensor óptico ni una escritura GATT nueva en ese instante. El flag estricto90s y la comprobación nocturna300s permanecen. Fallos, desconexión, recuperación y datos antiguos tienen prioridad sobre reposo.

## Revisión de botones y recorridos

Prueba real de navegador Chromium con HTTP simulado, sin llamadas al dispositivo. Inventario adjunto en AUDITORIA_UI_3_9_INVENTARIO_20261027.json.

| Grupo | Revisado |
|---|---|
| Luz/Curvas/Centro | Navegación, datos compartidos, orden, pérdida de comunicación y salida de simulación. |
| Mantenimiento | Iniciar, Comprobando luces, cancelar con Parar, subida, meseta, retorno y barra de progreso. El firmware de admisión conserva sus pruebas de ambas confirmaciones y fallos. |
| Gráfico | Zoom1/3/6/24h, comparación, selección y teclado conservados; simulación sin POST. |
| Editor | Aplicar, añadir, eliminar, mover minuto, deshacer/rehacer, cierre24:00 y límites de64puntos. |
| Perfil | Importación validada y checksum, exportación, guardado correcto, rechazo507 con editor conservado, restauración local de fábrica y deshacer. |
| Ajustes | Apertura/cierre/foco, sincronizar hora, controles avanzados; mensajes breves y nombres distintos para luz/memoria. |
| Wi-Fi | Escanear, cerrar durante escaneo, listar SSID con caracteres HTML como texto, guardar/conectar y olvidar en servidor simulado. |
| Alertas | Consulta de estado y habilitar/deshabilitar seguimiento; no se envía ningún Telegram real desde esta prueba. |
| Control | Reanudar curvas, ceder Chihiros y pausar; se conservan las confirmaciones que explican el efecto físico. |
| Informe | Descarga explícita, nombre único, bloqueo de duplicado y timeout de recepción. |
| Actualización | Archivo, tamaño, preflight rechazado/aceptado, preparación, SHA256, envío único, espera de reinicio y nueva salud; únicamente simulados en navegador. |

El inventario registra46 controles visibles enlazados y los recorridos principales. No afirma explorar todas las combinaciones posibles de pulsaciones, redes, firmwares o navegadores. Controles deshabilitados por diseño se inspeccionan sin forzar una orden física.

## Auditoría del proceso interno

- Estado: health de status utiliza la misma función de incidencia actual del Informe Maestro. Añade boot_id para verificar un reinicio; son lecturas, no escrituras ni órdenes BLE.
- Iluminación y seguridad: motor, worker Bluedroid, límites de conexión, cola única, prioridad OFF, recuperación y mantenimiento3.8 quedan fijados por hashes. Conserva inicio RAM, ambas confirmaciones antes de elevar luz, plazo45s y cancelación de resultados tardíos.
- Persistencia: LittleFS A/B, NVS de respaldo, orden de generaciones, archivos de staging preservados, ausencia de formato/grow y no-op permanecen idénticos. No se limpia ni borra NVS como efecto de esta auditoría.
- OTA: permanecen comprobación ESP32-S3/8MiB, SHA, partición, devolución de control, límites y rollback. No se reintroduce contraseña LAN ni firma obligatoria, de acuerdo con la decisión del propietario.
- Peticiones: endpoints heredados de laboratorio se conservan en firmware para compatibilidad y diagnóstico. Retirar JavaScript inaccesible no abre rutas nuevas ni activa pruebas automáticamente.
- Credenciales: se revisan escape de SSID, guardas de origen local y ausencia de publicación de claves. El informe privado y la copia cifrada no se incorporan al repositorio.

## Hallazgos que requieren trabajo separado

| Prioridad | Pendiente y motivo |
|---|---|
| P1 | Guardado de Wi-Fi usa dos claves NVS sucesivas; un fallo entre ambas podría dejar combinación parcial. Merece una transacción y una migración específica con pruebas de cortes, preservando la recuperación y los instaladores Sierra. No se cambia junto al renderizado. |
| P1 | El arranque conserva búsqueda inicial5s y ventanas30/180s. Explican la latencia y avisos blandos iniciales. Optimizarlo requiere revisar worker/SDK y probar pérdida real de radio; no se ocultan reduciendo alertas. |
| P1 | Falta sesión física de mantenimiento3.8.x/3.9 con el mecanismo nuevo de comprobación previa: el Informe89 acredita la sesión3.7, el90 acredita disponibilidad3.8, no ese recorrido completo posterior. |
| P1 | No hay sensor de luz. Un GATT aceptado no acredita brillo real ni detecta todos los fallos internos de una pantalla. Se conserva esta distinción en el diagnóstico. |
| P2 | Evaluar futuras simplificaciones de las rutas históricas del firmware y la nomenclatura interna v4/v5. Se mantienen para no romper instaladores, monitor o herramientas de recuperación. |
| P2 | Verificación visual en Safari del iPhone y funcionamiento prolongado con ciclo diurno completo. Chromium cubre lógica/maquetación, no equivale al hardware final. |

## Validación y límites

Suite host heredada y nuevas pruebas: modelo visual compartido, concurrencia HTTP, rutas, equivalencia JS/C++, admisión de mantenimiento, transporte, conservación del perfil y almacenamiento. Laboratorio LittleFS separado conserva sus132 cortes simulados y1000guardados idénticos. Chromium revisa320/390/768px sin desbordamiento y recorridos de botones sin errores JavaScript. Actions compila el SDK real y mantiene pruebas Sierra Python3.9; resultados concretos de la entrega constan en la PR.

No se ha ejecutado un apagado físico durante escritura ni un fallo real de Wi-Fi/BLE desde aquí. El despliegue queda para instalación manual. Tras instalar: comprobar versión3.9, gen10/38PV/32CH, mismas etiquetas en Luz/Centro; probar inicio y parada de mantenimiento y descargar un nuevo Informe Maestro. No hace falta importar otro perfil.
