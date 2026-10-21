"""Lectura HTTPS con identidad fijada por USB; sin sesión administradora."""
import hashlib
import hmac
import http.client
import json
import re
import ssl
from urllib.parse import urlsplit


class PollError(Exception):
    pass


def validate(data):
    if not isinstance(data, dict) or data.get("schema") != "cielo-monitor-v1":
        raise PollError("schema")
    # Copia cerrada, evita conservar accidentalmente secretos de otro endpoint.
    result = {"schema": "cielo-monitor-v1"}
    numeric = ("boot_id", "uptime_sec", "epoch", "heap_free", "heap_min", "largest_block",
               "ble_stack_free_bytes", "queue_depth", "worker_queue_depth", "off_failures")
    for key in numeric:
        v = data.get(key)
        if type(v) is not int or not 0 <= v <= 0xffffffff:
            raise PollError("schema")
        result[key] = v
    for key in ("clock_valid", "ble_ready", "worker_stalled", "ota", "off_pending", "ch_return_failed"):
        if type(data.get(key)) is not bool:
            raise PollError("schema")
        result[key] = data[key]
    v = data.get("firmware")
    if not isinstance(v, str) or not re.fullmatch(r"[A-Za-z0-9._+-]{1,80}", v):
        raise PollError("schema")
    result["firmware"] = v
    for lamp in ("pv", "ch"):
        v = data.get(lamp)
        if not isinstance(v, dict):
            raise PollError("schema")
        out = {}
        for key in ("expected", "ever_ok"):
            if type(v.get(key)) is not bool:
                raise PollError("schema")
            out[key] = v[key]
        for key in ("ok_age_sec", "consecutive_errors", "operations", "errors"):
            n = v.get(key)
            if type(n) is not int or not 0 <= n <= 0xffffffff:
                raise PollError("schema")
            out[key] = n
        result[lamp] = out
    p = data.get("profile", {})
    if not isinstance(p, dict):
        raise PollError("schema")
    result["profile"] = {}
    for key in ("generation", "pv_points", "ch_points"):
        n = p.get(key)
        if type(n) is not int or not 0 <= n <= (64 if "points" in key else 0xffffffff):
            raise PollError("schema")
        result["profile"][key] = n
    result["optical_feedback"] = None
    return result


class AtomClient:
    def __init__(self, url, fingerprint, token, timeout=5):
        self.url = urlsplit(url)
        if (self.url.scheme != "https" or not self.url.hostname or self.url.username
                or self.url.password or self.url.path not in ("", "/") or self.url.query or self.url.fragment):
            raise ValueError("ATOM requiere URL HTTPS sin rutas, credenciales ni parámetros")
        self.fingerprint = fingerprint.replace(":", "").lower()
        if not re.fullmatch("[0-9a-f]{64}", self.fingerprint) or not re.fullmatch("[0-9a-fA-F]{64}", token):
            raise ValueError("Faltan huella USB y token monitor de 64 caracteres hexadecimales")
        self.token, self.timeout = token, timeout

    def poll(self):
        # Pinning explícito de certificado autofirmado. No TOFU, no aceptar otro cert.
        # Conectamos y comprobamos DER ANTES de enviar la credencial de lectura.
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
        ctx.check_hostname = False
        ctx.verify_mode = ssl.CERT_NONE
        ctx.minimum_version = ssl.TLSVersion.TLSv1_2
        conn = http.client.HTTPSConnection(self.url.hostname, self.url.port or 443,
                                          context=ctx, timeout=self.timeout)
        try:
            conn.connect()
            actual = hashlib.sha256(conn.sock.getpeercert(binary_form=True)).hexdigest()
            if not hmac.compare_digest(actual, self.fingerprint):
                raise PollError("tls_identity")
            conn.request("GET", "/api/monitor", headers={"X-Cielo-Monitor": self.token,
                                                         "Cache-Control": "no-store", "Connection": "close"})
            response = conn.getresponse()
            if response.status != 200:
                raise PollError("access" if response.status in (401, 403) else "http")
            raw = response.read(8193)
            if len(raw) > 8192:
                raise PollError("size")
            return validate(json.loads(raw))
        except PollError:
            raise
        except (OSError, ValueError, http.client.HTTPException) as exc:
            # Nunca registrar excepciones con URL/token/certificado ni contenido remoto.
            raise PollError("transport") from None
        finally:
            conn.close()


class LocalAtomClient:
    """Explicit owner-selected LAN mode: HTTP, no certificate or read token."""
    def __init__(self, url, timeout=5):
        import ipaddress
        self.url = urlsplit(url)
        host = self.url.hostname or ""
        try:
            address = ipaddress.ip_address(host)
            local = address.version == 4 and (address.is_private or address.is_loopback)
        except ValueError:
            local = host.endswith(".local")
        if (self.url.scheme != "http" or not local or self.url.username
                or self.url.password or self.url.path not in ("", "/")
                or self.url.query or self.url.fragment):
            raise ValueError("Modo LAN: URL HTTP local sin ruta ni credenciales")
        self.timeout = timeout

    def poll(self):
        conn = http.client.HTTPConnection(self.url.hostname, self.url.port or 80, timeout=self.timeout)
        try:
            conn.request("GET", "/api/monitor", headers={"Cache-Control": "no-store", "Connection": "close"})
            response = conn.getresponse()
            if response.status != 200:
                raise PollError("http")
            raw = response.read(8193)
            if len(raw) > 8192:
                raise PollError("size")
            return validate(json.loads(raw))
        except PollError:
            raise
        except (OSError, ValueError, http.client.HTTPException):
            raise PollError("transport") from None
        finally:
            conn.close()


def configured_client(cfg):
    if cfg.get("local_lan") is True:
        return LocalAtomClient(cfg["atom_url"])
    import os
    return AtomClient(cfg["atom_url"], cfg["certificate_sha256"], os.environ.get("CIELO_MONITOR_TOKEN", ""))
