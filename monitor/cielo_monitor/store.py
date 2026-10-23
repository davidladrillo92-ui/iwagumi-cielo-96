"""SQLite local, retención 30 días y bandeja persistente de entregas acotada."""
import json
import os
import sqlite3
import time
from contextlib import contextmanager
from pathlib import Path
from .coverage import coverage


class Store:
    def __init__(self, path):
        self.path = str(Path(path).resolve())
        Path(self.path).parent.mkdir(parents=True, exist_ok=True, mode=0o700)
        fd = os.open(self.path, os.O_CREAT | os.O_RDWR, 0o600)
        os.close(fd)
        os.chmod(self.path, 0o600)
        with self.db() as c:
            version = c.execute("PRAGMA user_version").fetchone()[0]
            if version not in (0, 1):
                raise ValueError("Versión de histórico incompatible")
            c.executescript('''
            PRAGMA journal_mode=WAL;
            CREATE TABLE IF NOT EXISTS samples(ts INTEGER PRIMARY KEY, data TEXT, error TEXT);
            CREATE TABLE IF NOT EXISTS events(id INTEGER PRIMARY KEY, ts INTEGER, key TEXT, kind TEXT, text TEXT);
            CREATE INDEX IF NOT EXISTS events_time ON events(ts);
            CREATE TABLE IF NOT EXISTS meta(key TEXT PRIMARY KEY, value TEXT);
            CREATE TABLE IF NOT EXISTS outbox(id INTEGER PRIMARY KEY, ts INTEGER, provider TEXT,
                key TEXT, payload TEXT, attempts INTEGER DEFAULT 0, due INTEGER, UNIQUE(provider,key));
            PRAGMA user_version=1;
            ''')

    @contextmanager
    def db(self):
        c = sqlite3.connect(self.path, timeout=5)
        c.row_factory = sqlite3.Row
        try:
            yield c
            c.commit()
        except BaseException:
            c.rollback()
            raise
        finally:
            c.close()

    @staticmethod
    def meta_get(c, key, default=None):
        row = c.execute("SELECT value FROM meta WHERE key=?", (key,)).fetchone()
        return json.loads(row[0]) if row else default

    @staticmethod
    def meta_set(c, key, value):
        c.execute("INSERT OR REPLACE INTO meta VALUES(?,?)", (key, json.dumps(value)))

    @staticmethod
    def enqueue(c, now, provider, key, payload):
        c.execute("""INSERT INTO outbox(ts,provider,key,payload,due) VALUES(?,?,?,?,?)
            ON CONFLICT(provider,key) DO UPDATE SET ts=excluded.ts,payload=excluded.payload,
            attempts=CASE WHEN excluded.key IN ('state','observer') THEN outbox.attempts ELSE 0 END,
            due=CASE WHEN excluded.key IN ('state','observer') THEN outbox.due ELSE excluded.due END""", (now, provider, key, json.dumps(payload), now))
        c.execute("DELETE FROM outbox WHERE id NOT IN (SELECT id FROM outbox ORDER BY ts DESC,id DESC LIMIT 200)")

    def prune(self, now):
        with self.db() as c:
            for table in ("samples", "events"):
                c.execute(f"DELETE FROM {table} WHERE ts < ?", (now - 30*86400,))
            c.execute("DELETE FROM outbox WHERE ts < ?", (now - 86400,))
            c.execute("PRAGMA incremental_vacuum(64)")
        # DELETE reutiliza páginas; no VACUUM masivo ni escrituras flash en ATOM.

    def history(self, days, now=None):
        if days not in (7, 30):
            raise ValueError("Usa 7 o 30 días")
        now = int(time.time()) if now is None else now
        start = now - days*86400
        with self.db() as c:
            rows = c.execute("""SELECT (ts/3600)*3600 hour,COUNT(*) samples,
                SUM(data IS NOT NULL) reachable FROM samples WHERE ts>=? GROUP BY hour ORDER BY hour""", (start,)).fetchall()
            events = c.execute("SELECT * FROM events WHERE ts>=? ORDER BY ts DESC,id DESC LIMIT 1000", (start,)).fetchall()
            latest = c.execute("SELECT * FROM samples ORDER BY ts DESC LIMIT 1").fetchone()
            meta = self.meta_get(c, "state", {})
            count = c.execute("SELECT COUNT(*) FROM outbox").fetchone()[0]
            delivery = self.meta_get(c, "delivery", {})
            poll_seconds = self.meta_get(c, "poll_seconds", 60)
            all_samples = c.execute("SELECT ts,data FROM samples WHERE ts>=? ORDER BY ts", (start-poll_seconds,)).fetchall()
            cov = coverage(all_samples, start, now, poll_seconds)
        rows = [dict(r, expected=max(1, (min(now,r['hour']+3600)-max(start,r['hour']))//poll_seconds)) for r in rows]
        return {"days": days, "generated_at": now, "coverage": cov, "hours": rows,
                "events": [dict(x) for x in events], "events_limit": 1000,
                "latest": {"ts": latest["ts"], "data": json.loads(latest["data"]) if latest["data"] else None,
                           "error": latest["error"]} if latest else None,
                "conditions": meta.get("conditions", {}), "outbox_pending": count,
                "delivery": delivery, "optical_feedback": None}
