"""Alertas persistentes: debounce, recuperación verificada y antirrebote."""
import datetime as dt
import json
from zoneinfo import ZoneInfo
from .store import Store

LABELS = {
    "atom": "ATOMS3 no accesible: puede ser alimentación, red o acceso HTTPS; causa física desconocida",
    "pv": "Pez Verde sin respuesta BLE reciente",
    "ch": "Chihiros sin respuesta BLE reciente",
    "pv_errors": "Pez Verde acumula errores BLE repetidos",
    "ch_errors": "Chihiros acumula errores BLE repetidos",
    "ch_return": "No se ha completado el retorno BLE a My Chihiros; verificar la pantalla",
    "restarts": "ATOMS3 ha reiniciado repetidamente durante la última hora",
    "off": "POWER OFF Pez Verde pendiente con fallos; comprobar físicamente",
    "worker": "Trabajador Bluetooth no responde",
    "clock": "Reloj del ATOMS3 sin sincronizar",
    "memory": "Memoria interna baja en ATOMS3",
    "stale": "Telemetría del ATOMS3 no avanza",
}


class Engine:
    def __init__(self, store, providers=(), poll_seconds=60, daily_hour=9):
        self.store, self.providers = store, tuple(providers)
        self.poll_seconds, self.daily_hour = poll_seconds, daily_hour
        self.zone = ZoneInfo("Europe/Madrid")

    def _event(self, c, now, key, kind, text, notify=True):
        full = text + ". GATT no mide la luz física."
        c.execute("INSERT INTO events(ts,key,kind,text) VALUES(?,?,?,?)", (now, key, kind, full))
        if notify:
            # Colapsar mensajes pendientes de una misma incidencia: entrega el estado vigente.
            for provider in self.providers:
                Store.enqueue(c, now, provider, key, {"kind": kind, "key": key, "time": now, "text": full})

    def tick(self, now, data=None, error=None):
        with self.store.db() as c:
            state = Store.meta_get(c, "state", {"conditions": {}, "last_sample": None, "window": []})
            c.execute("INSERT OR REPLACE INTO samples VALUES(?,?,?)", (now, json.dumps(data) if data else None, error))
            conditions = {"atom": data is None}
            if data:
                previous = state.get("last_sample")
                same_boot = previous and previous["boot_id"] == data["boot_id"]
                boots = [t for t in state.get("restarts", []) if now-t < 3600]
                if previous and not same_boot:
                    boots.append(now)
                state["restarts"] = boots[-16:]
                conditions["restarts"] = len(boots) >= 3
                conditions["ch_return"] = data["ch_return_failed"]
                # Detecta replay/caché usando reloj del observador, no epoch de ATOM.
                if not same_boot or previous["uptime_sec"] != data["uptime_sec"]:
                    state["progress_at"] = now
                stale = now - state.get("progress_at", now) >= 180
                conditions["stale"] = stale
                state["last_sample"] = data
                state["window"] = [x for x in state.get("window", []) if now-x[0] <= 300 and x[1] == data["boot_id"]]
                old = state["window"][0] if state["window"] else None
                state["window"].append([now, data["boot_id"], data["pv"]["errors"], data["ch"]["errors"]])
                state["window"] = state["window"][-16:]
                conditions.update(worker=data["worker_stalled"] if data["ble_ready"] else None, clock=not data["clock_valid"],
                                  memory=data["heap_free"] < 60000)
                for index, lamp in enumerate(("pv", "ch")):
                    v = data[lamp]
                    active = v["expected"] and not data["ota"] and data["uptime_sec"] >= 180 and not stale
                    conditions[lamp] = ((not v["ever_ok"] or v["ok_age_sec"] > 650) if active else None)
                    delta = max(0, v["errors"] - old[index+2]) if old else 0
                    conditions[lamp+"_errors"] = (v["consecutive_errors"] >= 3 or delta >= 3) if active else None
                conditions["off"] = (data["off_pending"] and data["off_failures"] >= 3) if not stale else None
                if stale:
                    conditions = {"atom": False, "stale": True}
            # Ausencia de datos o modo cedido no equivale a recuperación de la lámpara.
            for key in LABELS:
                bad = conditions.get(key)
                st = state["conditions"].setdefault(key, {"active": False, "bad": 0, "good": 0, "last_notice": 0})
                if bad is None:
                    st["bad"] = st["good"] = 0
                    st["known"] = False
                    continue
                st["known"] = True
                st["bad"] = st["bad"]+1 if bad else 0
                st["good"] = st["good"]+1 if not bad else 0
                if bad and st["bad"] >= (1 if key in ("off", "ch_return") else 3):
                    first = not st["active"]
                    if first:
                        st["active"] = True
                        st["since"] = now
                    due = now-st["last_notice"] >= (600 if first else 3600)
                    if first or due:
                        self._event(c, now, key, "alert" if first else "reminder", LABELS[key], due)
                        if due:
                            st["last_notice"] = now
                            st["announced"] = True
                elif not bad and st["good"] >= 2 and st["active"]:
                    st["active"] = False
                    self._event(c, now, key, "recovery", "Recuperación de comunicación/estado: " + key, st.get("announced", False))
                    st["announced"] = False
            if "mqtt" in self.providers:
                Store.enqueue(c, now, "mqtt", "state", {"kind": "state", "time": now,
                    "reachable": data is not None, "conditions": state["conditions"], "sample": data})
            Store.meta_set(c, "state", state)
            self._daily(c, now)
        self.store.prune(now)

    def _daily(self, c, now):
        today = dt.datetime.fromtimestamp(now, self.zone)
        if today.hour < self.daily_hour:
            return
        day = today.date().isoformat()
        if Store.meta_get(c, "daily_sent") == day:
            return
        end = dt.datetime.combine(today.date(), dt.time(), self.zone)
        start = end - dt.timedelta(days=1)
        a, b = int(start.timestamp()), int(end.timestamp())
        row = c.execute("SELECT COUNT(*),SUM(data IS NOT NULL) FROM samples WHERE ts>=? AND ts<?", (a,b)).fetchone()
        events = c.execute("SELECT COUNT(*) FROM events WHERE ts>=? AND ts<? AND kind='alert'", (a,b)).fetchone()[0]
        expected = (b-a)//self.poll_seconds
        text = (f"Resumen CIELO {start.date()}: {row[0]} muestras de unas {expected} previstas; "
                f"{row[1] or 0} con acceso al ATOM; {events} incidencias. "
                "Los intervalos sin observador son desconocidos, no funcionamiento confirmado")
        self._event(c, now, "daily", "summary", text)
        Store.meta_set(c, "daily_sent", day)
