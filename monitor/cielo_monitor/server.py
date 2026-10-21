"""Panel privado. Bind loopback; acceso remoto por VPN/proxy TLS autenticado."""
import hmac
import json
from http.server import HTTPServer, BaseHTTPRequestHandler
from pathlib import Path
from urllib.parse import parse_qs, urlsplit


def serve(store, token, port=8765):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def do_GET(self):
            u = urlsplit(self.path)
            if u.path == "/":
                self.reply(200, Path(__file__).with_name("dashboard.html").read_bytes(), "text/html; charset=utf-8")
                return
            if not hmac.compare_digest(self.headers.get("Authorization", ""), "Bearer "+token):
                self.reply(401, b'{"error":"Acceso requerido"}')
                return
            if u.path != "/api/history":
                self.reply(404, b'{}')
                return
            try:
                days = int(parse_qs(u.query).get("days", ["7"])[0])
                self.reply(200, json.dumps(store.history(days), ensure_ascii=False).encode())
            except (ValueError, TypeError):
                self.reply(400, b'{"error":"Usa 7 o 30 dias"}')
            except Exception:
                self.reply(503, b'{"error":"Historico temporalmente no disponible"}')

        def reply(self, status, body, kind="application/json"):
            self.send_response(status)
            self.send_header("Content-Type", kind)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Content-Type-Options", "nosniff")
            self.send_header("Referrer-Policy", "no-referrer")
            self.send_header("X-Frame-Options", "DENY")
            self.send_header("Content-Security-Policy", "default-src 'self'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; connect-src 'self'; frame-ancestors 'none'")
            self.end_headers()
            self.wfile.write(body)
    server = HTTPServer(("127.0.0.1", port), Handler)
    server.timeout = 2
    return server
