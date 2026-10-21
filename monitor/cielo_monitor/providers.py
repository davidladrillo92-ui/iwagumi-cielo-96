"""Entregas opcionales fuera del ATOM. Nunca registran credenciales."""
import json
import ssl
import threading
import time
import urllib.error
import urllib.request
from .store import Store


class DeliveryError(Exception):
    def __init__(self, retry_after=0):
        self.retry_after = min(86400, max(0, retry_after))


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *args, **kwargs):
        return None


def https_request(url, data=None):
    # Configuración confiable del operador. TLS CA estándar, no redirecciones.
    opener = urllib.request.build_opener(NoRedirect(), urllib.request.HTTPSHandler(context=ssl.create_default_context()))
    request = urllib.request.Request(url, data=json.dumps(data).encode() if data is not None else None,
                                     headers={"Content-Type": "application/json"})
    try:
        with opener.open(request, timeout=8) as r:
            payload = r.read(32769)
            if len(payload) > 32768:
                raise DeliveryError()
            return payload
    except urllib.error.HTTPError as e:
        retry = 0
        if e.code == 429:
            try:
                retry = int(json.loads(e.read(4096)).get("parameters", {}).get("retry_after", 0))
            except (ValueError, TypeError):
                pass
        raise DeliveryError(retry) from None
    except (OSError, ValueError):
        raise DeliveryError() from None


class Telegram:
    def __init__(self, token, chat):
        self.token, self.chat = token, chat

    def send(self, payload):
        result = json.loads(https_request("https://api.telegram.org/bot"+self.token+"/sendMessage",
            {"chat_id": self.chat, "text": payload["text"][:3500], "disable_web_page_preview": True}))
        if not result.get("ok"):
            raise DeliveryError(int(result.get("parameters", {}).get("retry_after", 0)))

    def close(self):
        pass


class Heartbeat:
    def __init__(self, url):
        from urllib.parse import urlsplit
        u = urlsplit(url)
        if u.scheme != "https" or not u.hostname or u.username or u.fragment:
            raise ValueError("Heartbeat requiere una URL HTTPS privada sin usuario")
        self.url = url

    def send(self, payload):
        https_request(self.url)

    def close(self):
        pass


class MQTT:
    """MQTT 3.1.1 TLS, QoS1, LWT y discovery HA de solo lectura."""
    def __init__(self, cfg, password):
        import paho.mqtt.client as mqtt
        self.base = cfg.get("topic", "cielo/iwagumi96")
        self.ha = cfg.get("home_assistant", False)
        self.ready = threading.Event()
        self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=cfg.get("client_id", "cielo-observer"))
        self.client.max_queued_messages_set(32)
        self.client.max_inflight_messages_set(4)
        self.client.reconnect_delay_set(2, 60)
        self.client.tls_set(ca_certs=cfg.get("ca_file"))
        if cfg.get("username"):
            self.client.username_pw_set(cfg["username"], password)
        self.client.will_set(self.base+"/availability", "offline", qos=1, retain=True)
        self.client.on_connect = self._connect
        self.client.on_disconnect = lambda *args: self.ready.clear()
        self.client.connect_async(cfg["host"], cfg.get("port", 8883), keepalive=30)
        self.client.loop_start()
        self.discovery_due = True

    def _connect(self, client, userdata, flags, reason, properties):
        if not reason.is_failure:
            self.discovery_due = True
            self.ready.set()

    def _publish(self, topic, data, retain):
        info = self.client.publish(topic, data, qos=1, retain=retain)
        info.wait_for_publish(timeout=5)
        if not info.is_published():
            raise DeliveryError()

    def send(self, payload):
        if not self.ready.wait(1):
            raise DeliveryError()
        if self.discovery_due and self.ha:
            device = {"identifiers": [self.base], "name": "IWAGUMI CIELO", "manufacturer": "CIELO"}
            for name, label, template in (
                ("atom", "Acceso ATOMS3", "{{ 'accesible' if value_json.reachable else 'sin acceso' }}"),
                ("heap", "RAM libre ATOMS3", "{{ value_json.sample.heap_free if value_json.sample else none }}")):
                config = {"name": label, "unique_id": self.base.replace('/', '_')+'_'+name,
                    "device": device, "state_topic": self.base+"/state", "value_template": template,
                    "availability_topic": self.base+"/availability", "expire_after": 180}
                if name == "heap":
                    config["unit_of_measurement"] = "B"
                self._publish("homeassistant/sensor/"+self.base.replace('/', '_')+"/"+name+"/config", json.dumps(config), True)
            self.discovery_due = False
        self._publish(self.base+"/availability", "online", True)
        self._publish(self.base+("/state" if payload["kind"] == "state" else "/events"),
                      json.dumps(payload, ensure_ascii=False), payload["kind"] == "state")

    def close(self):
        try:
            if self.ready.is_set():
                self._publish(self.base+"/availability", "offline", True)
        finally:
            self.client.disconnect()
            self.client.loop_stop()


def deliver_one(store, providers, now):
    with store.db() as c:
        row = c.execute("SELECT * FROM outbox WHERE due<=? AND ts>=? ORDER BY due,id LIMIT 1", (now, now-86400)).fetchone()
    if not row:
        return False
    if row["provider"] not in providers:
        with store.db() as c:
            c.execute("DELETE FROM outbox WHERE id=?", (row["id"],))
        return True
    try:
        providers[row["provider"]].send(json.loads(row["payload"]))
    except Exception as e:
        delay = max(min(3600, 15 * 2**min(row["attempts"], 8)), getattr(e, "retry_after", 0))
        with store.db() as c:
            # No pisar un mensaje nuevo de la misma incidencia mientras se enviaba.
            c.execute("UPDATE outbox SET attempts=attempts+1,due=? WHERE id=? AND payload=?",
                      (now+delay, row["id"], row["payload"]))
            status = Store.meta_get(c, "delivery", {})
            status[row["provider"]] = {"ok": False, "at": now, "next_retry": now+delay}
            Store.meta_set(c, "delivery", status)
    else:
        with store.db() as c:
            c.execute("DELETE FROM outbox WHERE id=? AND payload=?", (row["id"], row["payload"]))
            status = Store.meta_get(c, "delivery", {})
            status[row["provider"]] = {"ok": True, "at": now}
            Store.meta_set(c, "delivery", status)
    return True


def delivery_loop(store, providers, stop):
    while not stop.is_set():
        try:
            work = deliver_one(store, providers, int(time.time()))
        except Exception:
            work = False  # DB ocupada/fallo: el hilo vuelve a intentarlo, sin secretos en logs.
        stop.wait(1 if work else 3)
