# IWAGUMI CIELO 9.6 — compilación en GitHub Actions

**Firmware experimental de desarrollo R4**, para M5Stack ATOMS3 Lite C124 (ESP32-S3, 8 MB flash, DIO). **NO INSTALAR EN EL ATOM** hasta completar la revisión BLE y probar la recuperación USB. Este proyecto recupera la compilación en la nube, no asegura todavía estabilidad física.

## Cómo compilar sin utilizar macOS Sierra

1. Conecta a ChatGPT la nueva cuenta GitHub desde **Configuración → Plugins → GitHub**, desconectando antes la cuenta suspendida. La autenticación requiere aprobación del usuario.
2. Crea un repositorio privado en la cuenta nueva, por ejemplo `iwagumi-cielo-96`, con rama `main` y README inicial. Indica a ChatGPT la URL del repositorio para subir los archivos por el conector, o incorpora manualmente **el contenido de esta carpeta** a la raíz del repositorio (no subas solo el archivo ZIP).
3. GitHub → repositorio → **Actions** → `Compilar IWAGUMI CIELO 9.6 R4 (ESP32-S3)` → **Run workflow** → ejecutar (también se activa con push en `main`).
4. Abre la ejecución. Si es verde, baja **Artifacts** → `CIELO_96_R4_DEV_BIN_SOLO_APLICACION_NO_INSTALAR`. El BIN producido es solo una aplicación, no la flash completa.
5. Si es rojo, baja **Artifacts** → `CIELO_96_R4_REGISTROS_COMPILACION`, o copia el texto del error. Envíalo a ChatGPT para corregir el fuente. No instales nada en el ATOM.

El builder usa Ubuntu 22.04, Python 3.11, PlatformIO 6.1.16 y `espressif32@6.8.1` (Arduino ESP32-S3); genera SHA-256 y rechaza un BIN que no encaje en `app0`/`app1`.

## Importante

- Este ZIP **no incluye** el volcado completo privado del ATOM, sus claves Wi-Fi, ni los binarios personales de respaldo.
- Las 9.5.3 y 9.5.2 siguen respaldadas fuera del repositorio.
- La cuenta anterior `rmw9mw9s49-blip/iwagumi-cielo` devuelve HTTP 403 por suspensión; el archivo YAML original no está disponible para copiar literalmente. Este pipeline es una **reconstrucción funcional**, no el archivo original recuperado.
- OTA automática y GitHub/Cloudflare como servicios del firmware siguen desactivados. GitHub es **solo una herramienta de compilación externa**; el ATOM seguirá usando la red local Bambu.
- Si la aplicación funciona bien en CI, hay que revisar las funciones BLE, el estado de AUTO y preparar restauración antes de instalar.
