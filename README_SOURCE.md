# Fuente canónica RC5

Esta rama se compila desde `src/`, `include/`, `web/` y `platformio.ini`.
El ZIP histórico ha sido retirado de esta rama; se conserva en el historial Git.
El flujo actual `.github/workflows/cielo-11-1-build.yml` genera un archivo completo
con `git archive HEAD` y un manifiesto con commit y hash del BIN, sin claves privadas.

Consultar [README](README.md) y [bloques 8/9](docs/B8_B9_RC5.md).
La fuente del observador está en `monitor/`; no es necesaria para las luces autónomas.
