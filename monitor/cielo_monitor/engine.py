"""Alertas persistentes: debounce, recuperación verificada y antirrebote."""
import datetime as dt
import json
from zoneinfo import ZoneInfo
from .store import Store
from .coverage import coverage

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


RECOVERIES = {
    "atom": "El ATOM vuelve a responder al observador",
    "pv": "Comunicación con Pez Verde recuperada",
    "ch": "Comunicación con Chihiros recuperada",
    "pv_errors": "Pez Verde vuelve a completar los envíos BLE",
    "ch_errors": "Chihiros vuelve a completar los envíos BLE",
    "off": "La orden de apagado de Pez Verde se ha completado por BLE",
    "worker": "El trabajador Bluetooth vuelve a responder",
    "clock": "El reloj del ATOM vuelve a estar sincronizado",
    "memory": "El ATOM vuelve a tener margen de memoria",
    "stale": "La telemetría vuelve a avanzar",
    "ch_return": "Retorno de control de Chihiros completado por BLE",
    "restarts": "Ya no se detectan reinicios repetidos en la última hora",
}
RESET_REASONS = {1: "encendido/alimentación", 2: "reset externo", 3: "reinicio de software",
                 4: "excepción del programa", 5: "watchdog de interrupción", 6: "watchdog de tarea",
                 7: "watchdog", 9: "caída de tensión"}
PHASES = {3: "descubrimiento", 4: "conexión", 5: "servicio GATT", 6: "característica GATT",
          7: "escritura GATT", 8: "desconexión", 9: "retorno My Chihiros"}

class Engine:
    def __init__(self, store, providers=(), poll_seconds=60, daily_hour=9):
        self.store, self.providers = store, tuple(providers)
        self.poll_seconds, self.daily_hour = poll_seconds, daily_hour
        self.zone = ZoneInfo("Europe/Madrid")

    def _event(self, c, now, key, kind, text, notify=True, delivery_key=None):
        headings = {"alert": "ALERTA", "recovery": "RECUPERACIÓN", "restart": "REINICIO",
                    "summary": "RESUMEN DIARIO", "no_data": "SIN DATOS", "restart_gap": "REINICIOS"}
        full = "CIELO · " + headings.get(kind, "AVISO") + "\n" + text.rstrip(".") + ". GATT no mide la luz física."
        c.execute("INSERT INTO events(ts,key,kind,text) VALUES(?,?,?,?)", (now, key, kind, full))
        if notify:
            # Colapsar mensajes pendientes de una misma incidencia: entrega el estado vigente.
            for provider in self.providers:
                Store.enqueue(c, now, provider, delivery_key or key, {"kind": kind, "key": key, "time": now, "text": full})

    def _restarts(self, c, now, data, state):
        previous = state.get("last_sample")
        if not previous:
            return False  # unknown baseline: never announce a historical boot at installation
        changed = data["boot_id"] != previous["boot_id"]
        # millis() rolls over after ~49.7 days. That is not a reboot.
        rollover = (previous["uptime_sec"] > 4294000 and data["uptime_sec"] < 3600
                    and now-state.get("last_sample_at", now) < 7200)
        backwards = data["uptime_sec"] < previous["uptime_sec"] and not rollover
        if not changed and not backwards:
            return False
        boots = [t for t in state.get("restarts", []) if 0 <= now-t < 3600]
        candidates = [e for e in data.get("recent_boots", [])
                      if previous["boot_id"] < e["boot_id"] <= data["boot_id"]] if changed else []
        candidates = sorted({e["boot_id"]: e for e in candidates}.values(), key=lambda e: e["boot_id"])
        if not candidates or candidates[-1]["boot_id"] != data["boot_id"]:
            candidates.append(dict(boot_id=data["boot_id"], previous_boot_id=previous["boot_id"],
                epoch=0, reset_reason=data.get("reset_reason", 0), ble_recovery=data.get("recovered_boot", False),
                phase=data.get("last_stall_phase", 0), device=data.get("last_stall_device", 255),
                stall_ms=data.get("last_stall_ms", 0), outcome=0, recovered_epoch=0))
        expected = data["boot_id"]-previous["boot_id"] if changed and data["boot_id"] > previous["boot_id"] else 1
        missing = max(0, expected-len(candidates))
        if missing:
            self._event(c, now, "restart", "restart_gap",
                str(missing)+" reinicios adicionales entre consultas; detalle fuera del historial de 8 arranques",
                delivery_key="restart-gap:"+str(data["boot_id"]))
        for e in candidates:
            boot = e["boot_id"]
            stamp = dt.datetime.fromtimestamp(e["epoch"], self.zone).strftime("%d/%m %H:%M:%S") if e.get("epoch") else "entre las dos últimas muestras"
            reason = RESET_REASONS.get(e.get("reset_reason"), "causa no disponible")
            text = f"Arranque {e.get('previous_boot_id', previous['boot_id'])} → {boot}, {stamp}. Motivo: {reason}"
            if e.get("ble_recovery"):
                lamp = {0: "Pez Verde", 1: "Chihiros"}.get(e.get("device"), "dispositivo desconocido")
                phase = PHASES.get(e.get("phase"), "fase desconocida")
                text += f"; recuperación BLE tras bloqueo de {e.get('stall_ms', 0)/1000:g} s en {lamp}, {phase}"
            elif backwards and not changed:
                text += "; uptime retrocedió sin cambiar boot_id; reinicio o contador inconsistente"
            text += ". No se atribuye a fallo BLE sin evidencia del supervisor"
            token = str(boot)+(":"+str(data["uptime_sec"]) if not changed else "")
            self._event(c, now, "restart", "restart", text, delivery_key="restart:"+token)
            boots.append(now)
        state["restarts"] = boots[-32:]
        state["restart_pending"] = data["boot_id"]
        return True

    def _restart_recovered(self, c, now, data, state):
        if state.get("restart_pending") != data["boot_id"]:
            return
        healthy = (data.get("boot_marked_healthy", data["uptime_sec"] >= 180) and data["ble_ready"]
                   and not data["worker_stalled"] and not data["off_pending"] and not data["ota"]
                   and not data["ch_return_failed"] and data["clock_valid"])
        for lamp in ("pv", "ch"):
            v = data[lamp]
            healthy = healthy and (not v["expected"] or (v["ever_ok"] and v["ok_age_sec"] <= data.get("communication_fresh_limit_sec",650) and not v["consecutive_errors"]))
        if healthy:
            boot = data["boot_id"]
            self._event(c, now, "restart", "recovery", f"Arranque {boot}: ATOM accesible y envíos BLE recientes registrados; comprueba visualmente las luces",
                        delivery_key="restart-recovered:"+str(boot))
            state.pop("restart_pending", None)

    def tick(self, now, data=None, error=None):
        with self.store.db() as c:
            Store.meta_set(c, "poll_seconds", self.poll_seconds)
            state = Store.meta_get(c, "state", {"conditions": {}, "last_sample": None, "window": []})
            c.execute("INSERT OR REPLACE INTO samples VALUES(?,?,?)", (now, json.dumps(data) if data else None, error))
            last_poll = state.get("last_poll")
            if last_poll is not None and now-last_poll > max(180, 3*self.poll_seconds):
                gap = now-last_poll-self.poll_seconds
                self._event(c, now, "observer", "no_data",
                    f"El observador vuelve tras {gap//60} min sin consultas; el estado de las luces durante ese intervalo es desconocido",
                    delivery_key="observer-gap:"+str(last_poll))
            state["last_poll"] = now
            conditions = {"atom": data is None}
            if data:
                previous = state.get("last_sample")
                restarted = self._restarts(c, now, data, state)
                same_boot = previous and previous["boot_id"] == data["boot_id"] and not restarted
                boots = [t for t in state.get("restarts", []) if 0 <= now-t < 3600]
                state["restarts"] = boots
                conditions["restarts"] = len(boots) >= 3
                self._restart_recovered(c, now, data, state)
                conditions["ch_return"] = data["ch_return_failed"]
                # Detecta replay/caché usando reloj del observador, no epoch de ATOM.
                if not same_boot or previous["uptime_sec"] != data["uptime_sec"]:
                    state["progress_at"] = now
                stale = now - state.get("progress_at", now) >= 180
                conditions["stale"] = stale
                state["last_sample"] = data
                state["last_sample_at"] = now
                state["window"] = [x for x in state.get("window", []) if now-x[0] <= 300 and x[1] == data["boot_id"]]
                old = state["window"][0] if state["window"] else None
                state["window"].append([now, data["boot_id"], data["pv"]["errors"], data["ch"]["errors"]])
                state["window"] = state["window"][-16:]
                conditions.update(worker=data["worker_stalled"] if data["ble_ready"] else None, clock=not data["clock_valid"],
                                  memory=data["heap_free"] < 60000)
                for index, lamp in enumerate(("pv", "ch")):
                    v = data[lamp]
                    active = v["expected"] and not data["ota"] and data["uptime_sec"] >= 180 and not stale
                    conditions[lamp] = ((not v["ever_ok"] or v["ok_age_sec"] > data.get("communication_fresh_limit_sec",650)) if active else None)
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
                    self._event(c, now, key, "recovery", RECOVERIES[key], st.get("announced", False))
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
        rows = c.execute("SELECT ts,data FROM samples WHERE ts>=? AND ts<? ORDER BY ts", (a-self.poll_seconds, b)).fetchall()
        inside = [r for r in rows if r["ts"] >= a]
        seen = sum(r["data"] is not None for r in inside)
        events = c.execute("SELECT ts,kind,text FROM events WHERE ts>=? AND ts<? ORDER BY ts", (a,b)).fetchall()
        count = lambda kind: sum(e["kind"] == kind for e in events)
        cov = coverage(rows, a, b, self.poll_seconds)
        expected = (b-a)//self.poll_seconds
        text = (f"{start.date()}: {len(inside)} consultas de unas {expected} previstas; {seen} con acceso al ATOM. "
                f"Reinicios detectados: {count('restart')}; recuperaciones: {count('recovery')}; incidencias: {count('alert')}. ")
        if not inside:
            text += "SIN DATOS: no se observó ese día; no equivale a un día sin fallos. "
        elif cov["no_observer_seconds"]:
            text += f"Sin observador: {cov['no_observer_seconds']//60} min. "
        if cov["unreachable_seconds"]:
            text += f"Sin acceso al ATOM durante consultas: aproximadamente {cov['unreachable_seconds']//60} min. "
        for x,y in cov["gaps"][:6]:
            text += "Sin datos " + dt.datetime.fromtimestamp(x,self.zone).strftime("%H:%M") + "–" + dt.datetime.fromtimestamp(y,self.zone).strftime("%H:%M") + ". "
        for e in [e for e in events if e["kind"] in ("restart", "restart_gap")][-6:]:
            text += dt.datetime.fromtimestamp(e["ts"],self.zone).strftime("%H:%M") + ": " + e["text"].split("\n",1)[-1].replace(". GATT no mide la luz física.", "") + ". "
        text += "Los intervalos sin observador son desconocidos, no funcionamiento confirmado"
        self._event(c, now, "daily", "summary", text)
        Store.meta_set(c, "daily_sent", day)
