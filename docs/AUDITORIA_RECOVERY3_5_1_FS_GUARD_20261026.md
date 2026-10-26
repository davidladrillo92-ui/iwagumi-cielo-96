# Auditoría RECOVERY3.5 y corrección 3.5.1

## Versión existente revisada

PR52, rama `dev/cielo-rc5-recovery3-5-spiffs-curvas-seguras-20261026`, fuente `5ff7f03f6ca348652cfc69e601a7a9e77d8b4626`. CI 40359076090 SUCCESS. Artefacto 12666364132: BIN 1.660.144 bytes, SHA256 `4f25ce2fdc2f14ab3c25b22ac20e662ba968f6c75bf5e7ec1b2096d5c93bcd9f`, versión `11.1.0-RC5-RECOVERY3.5-CURVAS-FS-CANDIDATE`. Build de merge `bc32e30acc71d17163dc9ca53bdb5e2f868ba23e`; src/main.cpp del archivo de fuentes coincide con la fuente de la rama.

Enfoque correcto para el fallo del informe 77: las curvas editables pasan a SPIFFS A/B en la partición existente de 1,5 MiB, sin depender de las 156 entradas NVS agregadas. Conserva curvas NVS generación 8 y la tabla de particiones. Solo inicializa una partición no montable cuando ha comprobado cada byte como 0xFF. Si contiene datos no montables, la preserva. Carga la generación válida más reciente entre ambos almacenes.

Las suites publicadas pasan. Sin embargo, el mock original no simulaba errores de lectura: comprobar error de rename no equivale a simular pérdida física de energía ni corrupción real de SPIFFS.

## Hallazgo reproducido: protección del perfil activo

1. Guardar generaciones 9, 10 y 11, con 11 activa y 10 de respaldo.
2. Una lectura transitoria de la ranura que contiene 11 devuelve cero bytes.
3. `read()` devuelve false, igual que para un archivo ausente.
4. `save()` toma el slot activo por vacío y lo elige como destino.
5. Elimina ese archivo y falla el renombrado del temporal.
6. La operación falla y el siguiente arranque solo recupera generación 10 (o la anterior NVS si tampoco queda FS válido).

Se ejecutó contra `cielo_profile_fs.h` original con un único fallo de lectura y fail_rename en el mock. No es evidencia de que haya ocurrido en el ATOM: es una regresión de lógica reproducible en host.

## Corrección 3.5.1

- Si un archivo existe y no se puede validar/leer, el guardado se detiene antes de eliminar slots. No confunde archivo ilegible con ausente.
- Relee y compara la copia activa exacta inmediatamente antes de retirar la inactiva.
- Rechaza candidatos con generación igual o anterior al FS actual. Un guardado que no se pudo confirmar debe inspeccionarse antes de repetirse.
- Marca cuándo rename ya publicó el nuevo archivo aunque falle la comprobación posterior; API e interfaz comunican la incertidumbre sin afirmar que no se escribió nada.
- Añade `curve_storage.loaded_backend` para distinguir de dónde procede el perfil activo, además del backend disponible.
- Mensajes legibles, sin JSON recortado. Una sola petición de guardado a la vez; conserva el editor pendiente cuando falla y amplía a 15 s el plazo de esa petición. Un timeout tampoco se interpreta como prueba de que no se guardó.
- Mantiene SPIFFS, NVS legado, Bluetooth persistente, perfiles, protocolos, OTA, particiones, Sierra y Telegram del PR52. No introduce la alternativa de reclamación NVS trabajada localmente antes de conocer PR52.

## Pruebas adicionales

`test_fs_active_guard.cpp`: reproducción anterior corregida; fallo en la relectura justo antes de eliminar; rechazo de generaciones antiguas; publicación seguida de lectura fallida; archivo presente con CRC inválido; partición con dirección incorrecta; byte no borrado en la última posición de la partición.

`test_fs_v5_profile.cpp`: JSON exacto V5 de 38 PV / 31 CH, guardado, lectura independiente y selección tras arranque, comparando todos los bytes. Fixture checksum `c4d2d529`.

`test_nvs_ui.js`: funciones reales API/saveProgram con HTTP 507, doble pulsación, error legible, editor pendiente conservado, éxito confirmado y resultado incierto. El nombre es heredado del fallo de origen; se aplica también al backend FS.

El resultado concreto de la suite completa C++/JS/Python y la compilación CI se registra en el PR junto al hash del BIN. No son pruebas en hardware.

## Condiciones antes de validar físicamente

- `curve_storage.fs_available=true`. Si UNMOUNTABLE_NONEMPTY_PRESERVED, no formatear ni seguir intentando el guardado.
- Guardar V5 una vez; comprobar 38 PV / 31 CH, generación 9 (si parte de 8), `SPIFFS_VERIFICADO` y `loaded_backend=SPIFFS_AB`.
- Exportar, comparar checksum/puntos, reiniciar controladamente y comprobar persistencia. Mantener copia externa.
- Volver a firmware 3.4 cargará la curva vieja NVS, no la nueva FS. El JSON nuevo debe conservarse para reimportarlo cuando corresponda.
- SPIFFS no garantiza protección absoluta frente a cortes físicos: ambas copias comparten filesystem. La documentación de Espressif advierte que una interrupción durante una operación puede corromperlo; no dar por aprobada esa prueba mediante mocks. Fuente: https://github.com/espressif/esp-idf/blob/v4.4.7/docs/en/api-reference/storage/spiffs.rst .
- NVS sigue teniendo la misma ocupación: esta solución separa el guardado de curvas, no repara genéricamente todo almacenamiento NVS.
