# Observador CIELO · B8

Servicio opcional e independiente. Solo lee al ATOMS3: **no cambia luz, reloj,
perfiles ni firmware**. Puede funcionar en un equipo siempre encendido de la LAN,
o en un servidor con VPN hacia ella. No abrir puertos del ATOM a Internet.
No hay servicio remoto contratado/activado ni mensajes enviados por esta entrega.

## Preparación

Requisitos: Python 3.11+, Linux/macOS moderno, acceso HTTPS al ATOM candidato.
Instalar el observador no instala firmware. RC3 no tiene `/api/monitor`:
primero debe validarse la candidata en banco; no configurar tokens administradores
como sustituto del token de lectura.

1. En CIELO, iniciar sesión y abrir **Ajustes → Alertas e historial**.
2. Crear clave de lectura (revoca la anterior). Se muestra una sola vez y no controla luces.
3. Copiar **la huella SHA256 del certificado que muestra el ATOM por USB**.
   No aceptar automáticamente una huella de la red. Una rotación de certificado
   requiere volver a comprobarla por USB; TLS incorrecto detiene la consulta antes de enviar token.
4. Copiar `config.example.json` a un archivo privado fuera de Git.
   Sustituir `certificate_sha256`, URL y ruta DB. No cambiar `.local` por `cielo.local`.
5. Crear variables privadas en un gestor de secretos o archivo `0600`, nunca en Git:

| Variable | Uso |
|---|---|
| `CIELO_MONITOR_TOKEN` | Clave de lectura generada por CIELO (64 hex) |
| `CIELO_PANEL_TOKEN` | Otra clave aleatoria >=32 caracteres para consultar el panel |
| `CIELO_TELEGRAM_TOKEN` / `CIELO_TELEGRAM_CHAT_ID` | Solo con Telegram opt-in |
| `CIELO_MQTT_PASSWORD` | Solo para broker con usuario |
| `CIELO_HEARTBEAT_URL` | Solo con servicio externo de heartbeat configurado |

No pegar claves en comandos compartidos, capturas o informes. Crear la clave del panel
con un generador local y guardarla como secreto. La contraseña administradora de CIELO
no se necesita en el observador.

```sh
cd monitor
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt  # solo necesario para MQTT/HA
# Cargar las variables privadas con el gestor/archivo local, sin imprimirlas.
.venv/bin/python -m cielo_monitor --config /ruta/privada/config.json --check
.venv/bin/python -m cielo_monitor --config /ruta/privada/config.json
```

`--check` comprueba URL/huella/token sin contactar al ATOM ni proveedores.
La muestra normal es cada 60 s (configurable 30–300); el ATOM limita a una cada 15 s.
La aplicación admite un solo observador propietario de una DB, mediante bloqueo de archivo.

## iPhone, acceso remoto y proveedores

- Panel en `http://127.0.0.1:8765` del host observador, con clave independiente.
  Para verlo desde iPhone, usar VPN y un proxy HTTPS autenticado hacia ese loopback.
  No publicar el puerto HTTP directamente. El HTML no contiene datos hasta autenticar.
  La clave del panel vive solo en memoria de esa pestaña; no se guarda en URLs/cookies/localStorage.
- Telegram: crear bot y abrir conversación desde el iPhone; configurar el chat propio,
  activar `telegram.enabled` y sus dos variables. El código usa `sendMessage` por HTTPS
  y respeta `retry_after` de Telegram. Verificar recepción real con un ensayo supervisado.
- MQTT: activar `mqtt.enabled`, broker TLS y credenciales de publicación restringidas al
  prefijo `cielo/iwagumi96/#` y, si corresponde, discovery. Puerto predeterminado 8883.
  No se permiten conexiones MQTT inseguras desde este adaptador.
- Home Assistant: `mqtt.home_assistant=true` publica discovery de sensores de acceso/RAM,
  availability del observador y expiración de 180 s. `cielo/iwagumi96/state` contiene
  estado completo; `.../events` transmite alertas/resúmenes. No hay temas de comandos.
  El availability MQTT indica conexión del observador, no que el ATOM o la luz estén funcionando.
- Heartbeat externo: registrar un ping periódico en un servicio independiente y configurar
  su URL HTTPS privada; activar `external_heartbeat`. Configurar allí período/gracia
  (por ejemplo 60 s + 180 s) y aviso a tu iPhone. Se pulsa tras procesar cada consulta,
  aunque ATOM no responda: vigila al **observador**, no duplica la alarma del ATOM.
  Detecta ausencia de observador o enlace externo; no permite saber si se cortó la electricidad.

Todo está desactivado por defecto. Los fallos de proveedores ocurren en un hilo externo
al muestreo, y todo el servicio está fuera del microcontrolador. Si muere este proceso,
CIELO sigue ejecutando su programación local, con las limitaciones BLE ya documentadas.

## Historial y avisos

La DB guarda una muestra por consulta, con hora del observador. No almacena SSID,
contraseñas, nombres BLE, IP del ATOM, RGB ni datos de descubrimiento.
Retención 30 días; vista de 7/30 con agregación horaria, último estado y hasta 1.000 eventos.
Los intervalos sin observador son **sin datos**. El CSV conserva todas las muestras del rango:

```sh
.venv/bin/python -m cielo_monitor --config /ruta/privada/config.json --export-csv 30 > /ruta/privada/historial.csv
```

Las alertas comienzan tras 3 muestras fallidas consecutivas (normalmente unos 2–3 min
según el momento del fallo). POWER OFF con >=3 fallos y retorno My Chihiros fallido
se avisan en la siguiente consulta. Recuperación requiere 2 muestras buenas;
no se infiere recuperación de una pantalla cuando CIELO ha cedido su control.
El control cedido puede funcionar bien con My Chihiros, pero este observador no lo mide.

Memoria <60.000 B, worker sin respuesta, reloj no válido, telemetría inmóvil y reinicios
repetidos tienen incidencias separadas. Ni conteos acumulados ni resets manuales aislados
se convierten automáticamente en fallos permanentes.

Bandeja persistente de 200 mensajes máximos, caducidad 24 h, reintentos 15 s hasta 1 h;
mensajes de una misma incidencia se sustituyen por su estado más reciente antes de entregar.
MQTT QoS1 y el reintento tras un timeout pueden producir duplicados inevitables de transporte;
se incluye clave/tipo/hora para deduplicar aguas abajo. No se promete entrega exactamente una vez.

## Instalación como servicio

`cielo-monitor.service` es una plantilla: crear usuario `cielo`, instalar fuente en
`/opt/cielo/monitor`, virtualenv en `/opt/cielo/venv`, configurar DB en
`/var/lib/cielo/history.sqlite`, y archivos `/etc/cielo/config.json` y `monitor.env`
privados. Ajustar rutas antes de instalar la unidad. `Restart=on-failure` reinicia
el observador; esto no corrige una interrupción de energía del host.

Respaldar la DB con la API SQLite backup o detener el servicio antes de copiarla:
no copiar solo `.sqlite` ignorando su WAL activo. Para retirar el observador, parar
el servicio, revocar su clave desde CIELO y desactivar las notificaciones en proveedores.
