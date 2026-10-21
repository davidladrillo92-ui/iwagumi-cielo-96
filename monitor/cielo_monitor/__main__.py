import argparse
import csv
import fcntl
import json
import os
import signal
import sys
import threading
import time
from pathlib import Path
from .transport import AtomClient, PollError
from .store import Store
from .engine import Engine
from .providers import Telegram, MQTT, Heartbeat, delivery_loop
from .server import serve


def env_required(name):
    value = os.environ.get(name, "")
    if not value:
        raise ValueError("Falta variable privada: "+name)
    return value


def load_config(path):
    cfg = json.loads(Path(path).read_text())
    if type(cfg.get("poll_seconds", 60)) is not int or not 30 <= cfg.get("poll_seconds", 60) <= 300:
        raise ValueError("poll_seconds: 30–300")
    if type(cfg.get("daily_hour", 9)) is not int or not 0 <= cfg.get("daily_hour", 9) <= 23:
        raise ValueError("daily_hour: 0–23")
    return cfg


def run(cfg):
    os.umask(0o077)
    token = env_required("CIELO_MONITOR_TOKEN")
    panel_token = env_required("CIELO_PANEL_TOKEN")
    if len(panel_token) < 32 or panel_token == token:
        raise ValueError("Clave del panel independiente, mínimo 32 caracteres")
    client = AtomClient(cfg["atom_url"], cfg["certificate_sha256"], token)
    store = Store(cfg.get("database", "private/history.sqlite"))
    # Un solo observador propietario de esta DB; evita duplicados/429 si se inicia dos veces.
    lock = open(store.path+".lock", "a")
    fcntl.flock(lock.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
    providers = {}
    if cfg.get("telegram", {}).get("enabled", False):
        providers["telegram"] = Telegram(env_required("CIELO_TELEGRAM_TOKEN"), env_required("CIELO_TELEGRAM_CHAT_ID"))
    if cfg.get("mqtt", {}).get("enabled", False):
        providers["mqtt"] = MQTT(cfg["mqtt"], env_required("CIELO_MQTT_PASSWORD") if cfg["mqtt"].get("username") else "")
    if cfg.get("external_heartbeat", False):
        providers["heartbeat"] = Heartbeat(env_required("CIELO_HEARTBEAT_URL"))
    # Heartbeat recibe exclusivamente pulsos del observador, nunca alertas ni secretos.
    alert_providers = tuple(k for k in providers if k != "heartbeat")
    engine = Engine(store, alert_providers, cfg.get("poll_seconds", 60), cfg.get("daily_hour", 9))
    server = serve(store, panel_token, cfg.get("port", 8765))
    stop = threading.Event()
    for sig in (signal.SIGINT, signal.SIGTERM):
        signal.signal(sig, lambda *args: stop.set())
    thread = threading.Thread(target=delivery_loop, args=(store, providers, stop), daemon=True)
    thread.start()
    web = threading.Thread(target=server.serve_forever, daemon=True)
    web.start()
    try:
        while not stop.is_set():
            start = time.monotonic()
            data, error = None, None
            try:
                data = client.poll()
            except PollError as e:
                error = str(e)
            now = int(time.time())
            engine.tick(now, data, error)
            if "heartbeat" in providers:
                with store.db() as c:
                    Store.enqueue(c, now, "heartbeat", "observer", {"kind": "heartbeat", "time": now})
            stop.wait(max(0, engine.poll_seconds-(time.monotonic()-start)))
    finally:
        stop.set()
        server.shutdown()
        server.server_close()
        thread.join(timeout=20)
        for provider in providers.values():
            provider.close()


def main():
    parser = argparse.ArgumentParser(description="Observador de CIELO, solo lectura")
    parser.add_argument("--config", default="config.json")
    parser.add_argument("--check", action="store_true", help="Validar configuración sin contactar servicios")
    parser.add_argument("--export-csv", choices=("7", "30"), help="Exportar muestras de DB; no contacta servicios")
    args = parser.parse_args()
    cfg = load_config(args.config)
    if args.export_csv:
        store = Store(cfg.get("database", "private/history.sqlite"))
        with store.db() as c:
            rows = c.execute("SELECT ts,data,error FROM samples WHERE ts>=? ORDER BY ts", (int(time.time())-int(args.export_csv)*86400,))
            writer = csv.writer(sys.stdout)
            writer.writerow(("epoch", "telemetry_json", "error_code"))
            writer.writerows(rows)
        return
    if args.check:
        AtomClient(cfg["atom_url"], cfg["certificate_sha256"], env_required("CIELO_MONITOR_TOKEN"))
        print("Configuración de lectura válida; no se ha conectado ni enviado ningún aviso.")
        return
    run(cfg)


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        # No imprimir URLs, datos de peticiones o credenciales por excepciones.
        print("Observador detenido: revisar configuración privada, red y permisos ("+type(e).__name__+").", file=sys.stderr)
        sys.exit(1)
