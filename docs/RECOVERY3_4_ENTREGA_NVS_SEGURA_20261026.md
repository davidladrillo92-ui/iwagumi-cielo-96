# CIELO 11.1.0 RC5 RECOVERY3.4 NVS — Informe de entrega candidata

Fecha 26/10/2026. Base física instalada anterior: RECOVERY3.3 BLE persistente, Informe Maestro 75, arranque 128.

## Funciones incorporadas

### 1. Guardado NVS de perfiles (P0)
- Se mantiene el formato de perfil original (sin migración), namespace `cielo96`, claves `curvesA` y `curvesB` y partición NVS de 0x5000 bytes.
- Validación de perfil, CRC y generación antes de escribir. Se leen y validan ambas copias. Se selecciona **la copia ausente o más antigua**, nunca la única válida.
- Comprobación previa no destructiva `nvs_get_stats`: si faltan incluso las entradas mínimas estimadas para el blob, se devuelve error 507. El cálculo es una **cota inferior**, no garantía de éxito por fragmentación/páginas.
- Escritura con API nativa `nvs_open` → `nvs_set_blob` → `nvs_commit`, registrando el **esp_err_t exacto** de la etapa que falla. Cierre del handle y relectura independiente con `nvs_get_blob`.
- Solo tras CRC, validación de curva y comparación binaria se sustituye el perfil activo en RAM. En caso de error no se toca la copia válida anterior.
- Mensajes diferenciados para: perfil inválido, estadísticas no disponibles, memoria insuficiente, apertura, escritura, confirmación, lectura y verificación. El endpoint devuelve `error`, `nvs_error_code`, `nvs_error_name`, `nvs_stage`, `previous_profile_preserved`.
- Informe Maestro: `nvs_diagnostics` con espacio, tamaño, slot, fase, código de error y contadores.

**Limitación importante:** esto soluciona la falta de visibilidad y la selección segura de copias, pero **no demuestra** que la NVS real del ATOM tenga espacio suficiente para guardar 38 puntos. No se borran datos, no se alteran particiones y no se promete éxito si la memoria está saturada.

### 2. Preservación de espacio NVS
- Registro de diagnóstico no crítico: se pospone si se está guardando una curva o las entradas NVS están por debajo de la reserva para un perfil, sin borrar el registro pendiente en RAM.
- Diario de reinicios: se pospone durante el guardado de curvas, pero se conserva su prioridad frente a registros no críticos; mantiene su limitación temporal de escritura.
- Contadores de escrituras pospuestas visibles en Informe Maestro. **Posponer escrituras no recupera entradas ya ocupadas**.

### 3. Recuperación de Chihiros
- Conserva enlaces persistentes de RECOVERY3.3 y retorno a My Chihiros mediante comando personalizado `0x03`.
- Una desconexión inesperada aislada de Chihiros no apaga Pez Verde. La recuperación de Chihiros y la liberación a la app deben verificarse físicamente.
- Pruebas de host para política de reintentos y ventanas de seguridad.

### 4. Pez Verde diurna
- Una desconexión inesperada de Pez Verde establece obligación de OFF prioritaria y bloquea el envío de RGB anterior hasta recuperar comunicación y confirmar OFF GATT.
- No confundir ACK GATT con comprobación óptica. La recuperación en pleno día requiere ensayo supervisado.
- Pruebas de host para condiciones de reanudación diurna.

### 5. Memoria y reinicios
- Mínimo de mayor bloque contiguo libre muestreado cada 15 min, sin escritura NVS adicional.
- El historial marca fechas ausentes o futuras como sospechosas sin reescribir la evidencia original; enlaza un boot PENDING con el siguiente boot recuperado cuando los datos lo sustentan.

## Invariantes

Sin cambios en puntos/horarios de las curvas V5, comandos de color, Bluedroid, cadencia de 30 s, apagado nocturno, Wi-Fi, OTA, monitor Telegram iMac Sierra y particiones.

## Pruebas antes de declarar estable

1. CI GitHub Actions: firmware PlatformIO ATOMS3 Lite C124, pruebas Python/C++ host y watchdog Sierra Python 3.9.
2. Con RECOVERY3.3 instalado, **exportar ambas curvas y descargar Informe Maestro antes de OTA**.
3. Instalar candidato de día, confirmar ambos enlaces, observar RGB y brillo, probar botón de devolución a My Chihiros.
4. Importar perfil nuevo; si NVS informa error, descargar Informe Maestro con `nvs_diagnostics`, **no borrar NVS**.
5. Si guarda, exportar, comparar todos los puntos, reiniciar controladamente y verificar persistencia.
6. Observar 24–72 h sin pérdida de enlaces, bloqueos, caídas de heap ni cambios de curva.

**Estado:** candidata en desarrollo. No instalada, no validada físicamente. No sustituye RECOVERY3.3 hasta superar pruebas.
