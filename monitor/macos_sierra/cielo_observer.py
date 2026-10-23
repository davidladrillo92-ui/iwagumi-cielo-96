#!/usr/bin/env python3
# IWAGUMI CIELO 4.0.0-SIERRA -- Python 3.9, macOS Sierra 10.12.6
# Solo consulta /api/monitor por HTTP en la LAN y envia mensajes por HTTPS a Telegram.
# No tiene endpoints de escritura del ATOM. No instala ni actualiza firmware.
import datetime
import fcntl
import getpass
import hashlib
import http.client
import ipaddress
import json
import os
import pathlib
import plistlib
import re
import shutil
import signal
import sqlite3
import ssl
import subprocess
import sys
import tempfile
import platform
import contextlib
import time
import urllib.error
import urllib.request
from urllib.parse import urlsplit

VERSION = '4.0.0-SIERRA'
LABEL = 'es.iwagumi.cielo.vigilancia'
APP_DIR = pathlib.Path.home() / 'Library' / 'Application Support' / 'IWAGUMI CIELO Vigilancia'
CONFIG = APP_DIR / 'config.json'
DB_FILE = APP_DIR / 'historial.sqlite'
LOG_FILE = APP_DIR / 'vigilancia.log'
AGENT = pathlib.Path.home() / 'Library' / 'LaunchAgents' / (LABEL + '.plist')
PY_SCRIPT = APP_DIR / 'cielo_observer.py'
POLL = 60
STOP = False
# Copia de CA Mozilla (certifi) incluida: verificamos Telegram por TLS,
# sin usar verificaciones inseguras ni depender de certificados antiguos del Mac.
TRUST_NAME = 'telegram_ca.pem'
TRUST_EXPECTED_SHA256 = '7cef540c6af3bf0a3c7d0070bd3fd7aae34f8aa805b408d1c27a01c8aeb305df'



def log(msg):
    # Ningun error muestra token, respuesta completa ni URLs de Telegram.
    stamp = datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    line = '[{}] {}'.format(stamp, msg)
    try:
        if sys.stdout.isatty(): print(line, flush=True)
    except OSError:
        pass
    try:
        if LOG_FILE.exists() and LOG_FILE.stat().st_size > 1200000:
            LOG_FILE.replace(LOG_FILE.with_suffix('.log.1'))
        with LOG_FILE.open('a', encoding='utf-8') as f:
            f.write(line + '\n')
    except OSError:
        pass


def make_private():
    APP_DIR.mkdir(parents=True, exist_ok=True)
    os.chmod(str(APP_DIR), 0o700)


def safe_write(path, data):
    make_private()
    tmp = path.with_suffix(path.suffix + '.tmp')
    with tmp.open('w', encoding='utf-8') as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
    os.chmod(str(tmp), 0o600)
    os.replace(str(tmp), str(path))


def valid_local_host(host):
    if not host:
        return False
    if host.endswith('.local') and re.fullmatch(r'[A-Za-z0-9-]+(?:\.[A-Za-z0-9-]+)*\.local', host):
        return True
    try:
        ip = ipaddress.ip_address(host)
        return isinstance(ip, ipaddress.IPv4Address) and (ip.is_private or ip.is_loopback)
    except ValueError:
        return False


def get_atom(base):
    u = urlsplit(base)
    if u.scheme != 'http' or not valid_local_host(u.hostname) or u.username or u.password or u.query or u.fragment or u.path not in ('', '/'):
        raise ValueError('Solo HTTP a una direccion local de CIELO')
    con = http.client.HTTPConnection(u.hostname, u.port or 80, timeout=6)
    try:
        con.request('GET', '/api/monitor', headers={'Cache-Control': 'no-store', 'Connection': 'close'})
        resp = con.getresponse()
        if resp.status != 200:
            raise ValueError('HTTP {}'.format(resp.status))
        raw = resp.read(8193)
        if len(raw) > 8192:
            raise ValueError('Respuesta de monitor demasiado grande')
        d = json.loads(raw)
        if not isinstance(d, dict) or d.get('schema') != 'cielo-monitor-v1':
            raise ValueError('La API del ATOM no corresponde al monitor B8')
        essential = ('boot_id','uptime_sec','heap_free','ble_ready','worker_stalled','clock_valid','pv','ch','ota')
        if not all(k in d for k in essential):
            raise ValueError('Faltan campos de monitor')
        for lamp in ('pv','ch'):
            if not isinstance(d[lamp], dict) or not all(k in d[lamp] for k in ('expected','ever_ok','ok_age_sec','consecutive_errors')):
                raise ValueError('Campos BLE incompletos')
        return d
    finally:
        con.close()



def telegram_tls_context():
    ca = pathlib.Path(__file__).resolve().parent / TRUST_NAME
    if not ca.is_file():
        raise FileNotFoundError('No se encuentra el paquete de certificados Telegram')
    with ca.open('rb') as fh:
        if hashlib.sha256(fh.read()).hexdigest() != TRUST_EXPECTED_SHA256:
            raise ValueError('El paquete de certificados esta incompleto o modificado')
    ctx = ssl.create_default_context(cafile=str(ca))
    # Python 3.9 soporta TLS >= 1.2; Telegram nunca se consulta en claro.
    if hasattr(ctx, 'minimum_version') and hasattr(ssl, 'TLSVersion'):
        ctx.minimum_version = ssl.TLSVersion.TLSv1_2
    return ctx


def telegram_http_opener():
    return urllib.request.build_opener(
        NoRedirect(), urllib.request.HTTPSHandler(context=telegram_tls_context()))


def classify_telegram_failure(exc):
    # No interpolar 'exc' ni imprimir URL, porque incluye el token de Telegram.
    if isinstance(exc, urllib.error.HTTPError):
        if exc.code == 401:
            return 'TELEGRAM_401: token revocado, incorrecto o sin autorización'
        if exc.code == 409:
            return 'TELEGRAM_409: otro proceso esta consultando el mismo bot'
        if exc.code == 429:
            return 'TELEGRAM_429: demasiadas solicitudes; espera y reintenta'
        return 'TELEGRAM_HTTP_{}'.format(exc.code)
    reason = exc.reason if isinstance(exc, urllib.error.URLError) else exc
    if isinstance(reason, ssl.SSLCertVerificationError):
        return 'CERTIFICADO_TLS: la cadena de confianza no se puede verificar'
    if isinstance(reason, ssl.SSLError):
        return 'TLS_NEGOCIACION: protocolo/cifrado SSL no compatible'
    if isinstance(reason, (TimeoutError,)):
        return 'TIMEOUT: Telegram no ha respondido a tiempo'
    if isinstance(reason, OSError):
        import socket
        if isinstance(reason, socket.gaierror):
            return 'DNS: no se puede resolver api.telegram.org'
        if isinstance(reason, ConnectionRefusedError):
            return 'CONEXION_RECHAZADA: comprobar proxy o cortafuegos'
        if isinstance(reason, ConnectionResetError):
            return 'CONEXION_REINICIADA: el enlace se ha interrumpido'
        return 'RED_{}: conexion no disponible'.format(getattr(reason, 'errno', 'desconocido'))
    if isinstance(exc, ValueError):
        return 'RESPUESTA_O_ARCHIVO_INVALIDO: comprobar certificados o API Token'
    return 'FALLO_{}: conexion no completada'.format(type(exc).__name__)


def check_telegram_https(verbose=True):
    # Sonda SIN token y SIN modificar Telegram ni el ATOM.
    try:
        opener = telegram_http_opener()
        req = urllib.request.Request('https://api.telegram.org/',
                                    headers={'Cache-Control':'no-store'}, method='GET')
        with opener.open(req, timeout=12) as resp:
            resp.read(64)
    except urllib.error.HTTPError as ex:
        # Un HTTP 404/405/301 significa que TLS y la red funcionan; no usamos redirecciones.
        if ex.code not in (301, 302, 403, 404, 405):
            if verbose: print('Prueba Telegram: {}'.format(classify_telegram_failure(ex)))
            return False
    except Exception as ex:
        if verbose: print('Prueba Telegram: {}'.format(classify_telegram_failure(ex)))
        return False
    if verbose: print('Prueba Telegram: HTTPS y certificado verificados correctamente (sin token).')
    return True


def diagnosis():
    print('IWAGUMI CIELO - COMPROBACION DE TELEGRAM (SIN TOKEN)')
    print('Python:', sys.version.split()[0])
    print('OpenSSL:', ssl.OPENSSL_VERSION)
    print('Fecha local:', datetime.datetime.now().strftime('%Y-%m-%d %H:%M'))
    print('UTC:', datetime.datetime.utcnow().strftime('%Y-%m-%d %H:%M'))
    ok = check_telegram_https()
    print('La prueba no consulta el bot, no muestra contraseñas y no altera las luces.')
    if not ok:
        print('Si falla: revisa hora y fecha en macOS, acceso HTTPS y proxy/firewall.')
        print('No uses --insecure ni desactives la validacion TLS.')
    return 0 if ok else 1

class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *a, **k):
        return None


def telegram(token, method, payload=None):
    if not re.fullmatch(r'[0-9]{5,20}:[A-Za-z0-9_-]{20,128}', token):
        raise ValueError('Formato de token de Telegram incorrecto')
    url = 'https://api.telegram.org/bot{}/{}'.format(token, method)
    body = json.dumps(payload or {}).encode('utf-8')
    req = urllib.request.Request(url, data=body, headers={'Content-Type': 'application/json'}, method='POST')
    opener = telegram_http_opener()
    with opener.open(req, timeout=17) as resp:
        content = resp.read(32769)
    if len(content) > 32768:
        raise ValueError('Respuesta de Telegram demasiado grande')
    result = json.loads(content)
    if not result.get('ok'):
        raise ValueError('Telegram ha rechazado la solicitud')
    return result.get('result')


def send_message(cfg, msg):
    telegram(cfg['telegram_token'], 'sendMessage', {'chat_id': cfg['chat_id'], 'text': msg[:3500], 'disable_web_page_preview': True})


def make_db():
    make_private()
    c = sqlite3.connect(str(DB_FILE), timeout=15)
    c.execute('PRAGMA busy_timeout=15000')
    c.execute('CREATE TABLE IF NOT EXISTS samples (ts INTEGER PRIMARY KEY, ok INTEGER, firmware TEXT, uptime INTEGER, heap INTEGER, pv_age INTEGER, ch_age INTEGER, error TEXT)')
    c.execute('CREATE TABLE IF NOT EXISTS events (ts INTEGER, key TEXT, kind TEXT, message TEXT)')
    c.execute('CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, val TEXT)')
    c.execute('CREATE TABLE IF NOT EXISTS outbox (key TEXT PRIMARY KEY, msg TEXT, due INTEGER, attempts INTEGER)')
    c.commit()
    c.close()
    os.chmod(str(DB_FILE), 0o600)


def read_meta(db, name, default):
    row = db.execute('SELECT val FROM meta WHERE key=?', (name,)).fetchone()
    try:
        return json.loads(row[0]) if row else default
    except (TypeError, ValueError):
        return default


def meta(db, name, value):
    db.execute('INSERT OR REPLACE INTO meta(key,val) VALUES(?,?)', (name, json.dumps(value)))


def outbox(db, key, msg, now):
    db.execute('INSERT OR REPLACE INTO outbox(key,msg,due,attempts) VALUES(?,?,?,0)', (key,msg,now))


def flush_outbox(db, cfg, now):
    row = db.execute('SELECT key,msg,attempts FROM outbox WHERE due<=? ORDER BY due LIMIT 1', (now,)).fetchone()
    if not row:
        return
    key, msg, attempts = row
    try:
        send_message(cfg,msg)
    except Exception as ex:
        # Nunca registrar URLs, token, respuesta de Telegram, excepcion HTTP ni datos privados.
        delay = min(3600, 15 * 2**min(8,attempts))
        if isinstance(ex,urllib.error.HTTPError) and ex.code==429:
            try:delay=max(delay,min(86400,int(json.loads(ex.read(4096)).get('parameters',{}).get('retry_after',0))))
            except (ValueError,TypeError):pass
        db.execute('UPDATE outbox SET attempts=?,due=? WHERE key=? AND msg=?', (attempts+1, now+delay, key, msg))
        log('Telegram no pudo entregar un mensaje; reintento previsto. Tipo: '+type(ex).__name__)
    else:
        db.execute('DELETE FROM outbox WHERE key=? AND msg=?', (key,msg))
        log('Aviso Telegram entregado: '+key)


def issue(db, st, key, bad, now, label, urgent=False):
    item = st.setdefault(key, {'bad':0,'good':0,'active':False,'last_alert':0})
    if bad is None:
        item['bad'] = 0
        item['good'] = 0
        return
    item['bad'] = item['bad'] + 1 if bad else 0
    item['good'] = item['good'] + 1 if not bad else 0
    threshold = 1 if urgent else 3
    if bad and item['bad'] >= threshold:
        new = not item['active']
        if new:
            item['active']=True
        if new or now-item.get('last_alert',0)>=3600:
            message = ('🚨 CIELO · ALERTA\n' if new else '⚠️ CIELO · RECORDATORIO\n') + label + '\nComprobar la pantalla físicamente: GATT no mide la luz real.'
            outbox(db,key,message,now)
            db.execute('INSERT INTO events(ts,key,kind,message) VALUES(?,?,?,?)',(now,key,'alerta',label))
            item['last_alert']=now
            log('Incidencia: '+key)
    elif not bad and item['good']>=2 and item['active']:
        item['active']=False
        recovered={'off':'Orden de apagado de Pez Verde completada por BLE.', 'pv':'Comunicación con Pez Verde recuperada.', 'ch':'Comunicación con Chihiros recuperada.', 'atom':'El ATOM vuelve a responder al iMac.', 'worker':'El trabajador Bluetooth vuelve a responder.'}.get(key,'La telemetría vuelve a estar normal: '+key)
        message='✅ CIELO · RECUPERACIÓN\n'+recovered+'\nNo es una comprobación óptica.'
        outbox(db,key,message,now)
        db.execute('INSERT INTO events(ts,key,kind,message) VALUES(?,?,?,?)',(now,key,'recuperacion',label))
        log('Recuperación: '+key)


def record_sample(db, now, data, error):
    if data is None:
        db.execute('INSERT OR REPLACE INTO samples(ts,ok,error) VALUES(?,0,?)',(now,error))
    else:
        db.execute('INSERT OR REPLACE INTO samples(ts,ok,firmware,uptime,heap,pv_age,ch_age,error) VALUES(?,?,?,?,?,?,?,NULL)',
            (now,1,str(data.get('firmware',''))[:80],int(data['uptime_sec']),int(data['heap_free']),int(data['pv']['ok_age_sec']),int(data['ch']['ok_age_sec'])))


def record_event(db,key,kind,text,now,notify=True):
    # A restart and its recovery have distinct persistent delivery keys.
    if db.execute('SELECT 1 FROM events WHERE key=? AND kind=?',(key,kind)).fetchone():return
    db.execute('INSERT INTO events(ts,key,kind,message) VALUES(?,?,?,?)',(now,key,kind,text))
    if notify:outbox(db,key,'CIELO · '+kind.upper()+'\n'+text+'\nGATT no mide la luz física.',now)


def detect_restarts(db,st,data,now):
    old=st.get('_last') or {}
    if old.get('boot') is None:return
    boot=data['boot_id'];previous=old['boot'];changed=previous!=boot
    rollover=old.get('uptime',0)>4294000 and data['uptime_sec']<3600 and now-st.get('_last_sample_at',now)<7200
    backwards=data['uptime_sec']<old.get('uptime',0) and not rollover
    if not changed and not backwards:return
    entries=data.get('recent_boots',[])
    entries=entries if isinstance(entries,list) and len(entries)<=8 else []
    known={e['boot_id']:e for e in entries if isinstance(e,dict) and type(e.get('boot_id')) is int and previous<e['boot_id']<=boot} if changed else {}
    known.setdefault(boot,{'boot_id':boot,'previous_boot_id':previous,'reset_reason':data.get('reset_reason',0),
        'ble_recovery':data.get('recovered_boot',False),'phase':data.get('last_stall_phase',0),
        'device':data.get('last_stall_device',255),'stall_ms':data.get('last_stall_ms',0)})
    count=boot-previous if changed and boot>previous else 1
    missing=max(0,count-len(known))
    if missing:record_event(db,'restart-gap:'+str(boot),'reinicios_sin_detalle',str(missing)+' reinicios adicionales entre consultas; detalle fuera del historial disponible.',now)
    recent=[t for t in st.get('_restarts',[]) if 0<=now-t<3600]
    reasons={1:'encendido/alimentación',2:'reset externo',3:'reinicio de software',4:'excepción',5:'watchdog de interrupción',6:'watchdog de tarea',7:'watchdog',9:'caída de tensión'}
    for identity,e in sorted(known.items()):
        epoch=e.get('epoch',0)
        stamp=datetime.datetime.fromtimestamp(epoch).strftime('%d/%m %H:%M:%S') if type(epoch) is int and 0<epoch<0xffffffff else 'entre las últimas consultas'
        text='Arranque {} → {}, {}. Motivo: {}.'.format(e.get('previous_boot_id',previous),identity,stamp,reasons.get(e.get('reset_reason'),'no disponible en este firmware'))
        if e.get('ble_recovery'):
            lamp={0:'Pez Verde',1:'Chihiros'}.get(e.get('device'),'dispositivo desconocido')
            phase={4:'conexión',5:'servicio GATT',6:'característica GATT',7:'escritura GATT',8:'desconexión'}.get(e.get('phase'),'fase '+str(e.get('phase',0)))
            text+=' Supervisor BLE: {}, {}, {} ms.'.format(lamp,phase,e.get('stall_ms',0))
        elif backwards and not changed:text+=' Uptime retrocedió sin cambiar boot_id; reinicio o contador inconsistente.'
        key='restart:'+str(identity)+(':'+str(data['uptime_sec']) if not changed else '')
        record_event(db,key,'reinicio',text,now)
        if identity!=boot and e.get('outcome')==1:
            record_event(db,'restart-recovered:'+str(identity),'recuperacion','Arranque {}: el registro del ATOM confirma recuperación de comunicación antes del siguiente arranque.'.format(identity),now)
        recent.append(now)
    st['_restarts']=recent[-32:];st['_restart_pending']=boot


def check_restart_recovery(db,st,data,now):
    if st.get('_restart_pending')!=data['boot_id']:return
    healthy=(data.get('boot_marked_healthy',data['uptime_sec']>=180) and data['ble_ready'] and not data['worker_stalled']
        and data['clock_valid'] and not data.get('off_pending') and not data.get('ch_return_failed') and not data['ota'])
    for lamp in ('pv','ch'):
        v=data[lamp]
        healthy=healthy and (not v['expected'] or (v['ever_ok'] and v['ok_age_sec']<=650 and not v['consecutive_errors']))
    if healthy:
        record_event(db,'restart-recovered:'+str(data['boot_id']),'recuperacion','Arranque {}: ATOM accesible y comunicación de las pantallas restablecida.'.format(data['boot_id']),now)
        st.pop('_restart_pending',None)


def launch(args):
    return subprocess.run(['/bin/launchctl']+args,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode


def agent_plist(python):
    params=[python,str(PY_SCRIPT),'run']
    if pathlib.Path('/usr/bin/caffeinate').exists():params=['/usr/bin/caffeinate','-i']+params
    return {'Label':LABEL,'ProgramArguments':params,'WorkingDirectory':str(APP_DIR),
        'RunAtLoad':True,'KeepAlive':True,'ThrottleInterval':30,
        'EnvironmentVariables':{'PYTHONIOENCODING':'utf-8','TZ':'CET-1CEST,M3.5.0,M10.5.0/3'},
        'StandardOutPath':str(APP_DIR/'servicio.stdout.log'),'StandardErrorPath':str(APP_DIR/'servicio.stderr.log')}


def check_existing():
    # Inspect, never execute an old script or a private configuration as code.
    if not all(p.is_file() and not p.is_symlink() for p in (CONFIG,PY_SCRIPT,DB_FILE,AGENT,APP_DIR/TRUST_NAME)):
        raise ValueError('No se identifica una instalación Sierra V3/V4 completa')
    cfg=json.loads(CONFIG.read_text(encoding='utf-8'))
    if not isinstance(cfg,dict) or not all(k in cfg for k in ('atom_url','telegram_token','chat_id')):
        raise ValueError('Configuración previa incompleta')
    if not re.fullmatch(r'[0-9]{5,20}:[A-Za-z0-9_-]{20,128}',str(cfg['telegram_token'])):
        raise ValueError('Formato de token guardado no válido')
    if not re.fullmatch(r'-?[0-9]{1,20}',str(cfg['chat_id'])):raise ValueError('Chat previo no identificado')
    u=urlsplit(cfg['atom_url'])
    if u.scheme!='http' or not valid_local_host(u.hostname) or u.path not in ('','/') or u.username or u.password or u.query or u.fragment:
        raise ValueError('Dirección previa del ATOM no válida')
    with AGENT.open('rb') as f:agent=plistlib.load(f)
    if agent.get('Label')!=LABEL or str(PY_SCRIPT) not in agent.get('ProgramArguments',[]):
        raise ValueError('El LaunchAgent no corresponde al vigilante Sierra')
    with sqlite3.connect('file:'+str(DB_FILE)+'?mode=ro',uri=True) as db:
        required={'samples':{'ts','ok','firmware','uptime','heap','pv_age','ch_age','error'},
                  'events':{'ts','key','kind','message'},'meta':{'key','val'},'outbox':{'key','msg','due','attempts'}}
        for table,columns in required.items():
            present={r[1] for r in db.execute('PRAGMA table_info('+table+')')}
            if not columns<=present:raise ValueError('Formato del histórico no compatible')
    return cfg


@contextlib.contextmanager
def stopped_observer():
    launch(['unload',str(AGENT)])
    fh=open(str(APP_DIR/'vigilancia.lock'),'a');deadline=time.monotonic()+30
    try:
        while True:
            try:fcntl.flock(fh.fileno(),fcntl.LOCK_EX|fcntl.LOCK_NB);break
            except (IOError,OSError):
                if time.monotonic()>=deadline:raise RuntimeError('El vigilante anterior no se ha detenido')
                time.sleep(0.2)
        yield
    finally:fh.close()


def update_installation():
    if sys.platform!='darwin' or sys.version_info<(3,9):
        print('Esta actualización necesita macOS y Python 3.9.');return 1
    check_existing();telegram_tls_context() # verify the bundled CA before touching the service
    os.umask(0o077);make_private()
    source=pathlib.Path(__file__).resolve().parent
    if source==APP_DIR.resolve():
        print('Ejecuta ACTUALIZAR.command desde la carpeta descargada.');return 1
    with open(str(APP_DIR/'actualizar.lock'),'a') as guard:
        fcntl.flock(guard.fileno(),fcntl.LOCK_EX|fcntl.LOCK_NB)
        stamp=datetime.datetime.now().strftime('%Y%m%d_%H%M%S_%f')
        backup=APP_DIR/'backups'/stamp;backup.mkdir(parents=True,mode=0o700)
        stage=pathlib.Path(tempfile.mkdtemp(prefix='v4_preparada_',dir=str(APP_DIR)))
        changed=False
        try:
            for name in ('cielo_observer.py',TRUST_NAME):
                shutil.copy2(str(source/name),str(stage/name))
            compile((stage/'cielo_observer.py').read_text(encoding='utf-8'),'cielo_observer.py','exec')
            with stopped_observer():
                for path in (CONFIG,PY_SCRIPT,APP_DIR/TRUST_NAME,AGENT):
                    shutil.copy2(str(path),str(backup/path.name))
                with sqlite3.connect(str(DB_FILE)) as current,sqlite3.connect(str(backup/DB_FILE.name)) as saved:current.backup(saved)
                for name in ('cielo_observer.py',TRUST_NAME):
                    changed=True;os.replace(str(stage/name),str(APP_DIR/name));os.chmod(str(APP_DIR/name),0o600)
                with (stage/'agent.plist').open('wb') as f:plistlib.dump(agent_plist(str(pathlib.Path(sys.executable).resolve())),f)
                os.replace(str(stage/'agent.plist'),str(AGENT));os.chmod(str(AGENT),0o600)
            if launch(['load','-w',str(AGENT)]):raise RuntimeError('No se pudo cargar el LaunchAgent')
            print('Vigilante '+VERSION+' instalado con arranque automático.')
            print('Token, chat, IP e histórico anteriores conservados. No se ha enviado ninguna prueba de Telegram.')
            print('Abre CONSULTAR_ESTADO.command tras un minuto para confirmar la primera consulta.')
            return 0
        except BaseException:
            if changed:
                with stopped_observer():restore_backup(backup)
            launch(['load','-w',str(AGENT)])
            print('Actualización no completada; se conserva o restaura la instalación anterior.')
            raise
        finally:shutil.rmtree(str(stage),ignore_errors=True)


def restore_backup(backup):
    # Code/service rollback does not rewind the live history or bot configuration.
    for name in ('cielo_observer.py',TRUST_NAME):
        temporary=APP_DIR/(name+'.restore');shutil.copy2(str(backup/name),str(temporary))
        os.chmod(str(temporary),0o600);os.replace(str(temporary),str(APP_DIR/name))
    temporary=AGENT.with_suffix('.restore');shutil.copy2(str(backup/AGENT.name),str(temporary))
    os.chmod(str(temporary),0o600);os.replace(str(temporary),str(AGENT))


def rollback_installation():
    if sys.platform!='darwin':return 1
    check_existing()
    backups=sorted((APP_DIR/'backups').glob('*'),reverse=True)
    backups=[p for p in backups if all((p/n).is_file() for n in ('cielo_observer.py',TRUST_NAME,AGENT.name))]
    if not backups:
        print('No hay copia previa para restaurar.');return 1
    with open(str(APP_DIR/'actualizar.lock'),'a') as guard:
        fcntl.flock(guard.fileno(),fcntl.LOCK_EX|fcntl.LOCK_NB)
        try:
            with stopped_observer():restore_backup(backups[0])
        finally:launch(['load','-w',str(AGENT)])
    print('Código anterior restaurado; configuración e historial actuales conservados.');return 0


def evaluate(db, st, data, now):
    issue(db,st,'atom',data is None,now,'ATOM sin acceso desde el iMac durante tres comprobaciones. Puede ser Wi-Fi, corriente o software.')
    if data is None:
        # No interpretar falta de datos como recuperación de ninguna pantalla.
        for key in ('pv','ch','pv_err','ch_err','worker','clock','memory','off','return','stale'):
            issue(db,st,key,None,now,'')
        return
    detect_restarts(db, st, data, now)
    boot = data['boot_id']
    old = st.get('_last') or {}
    prev_boot = old.get('boot')
    recent = [t for t in st.get('_restarts',[]) if now-t<=3600]
    st['_restarts']=recent[-20:]
    issue(db,st,'restarts',len(recent)>=3,now,'ATOM reiniciado al menos tres veces en una hora.')
    if not old or old.get('boot')!=boot or old.get('uptime')!=data['uptime_sec']:
        st['_last_progress']=now
    stale = now - st.get('_last_progress',now) >=180
    issue(db,st,'stale',stale,now,'Telemetría congelada: el reloj de funcionamiento del ATOM no avanza.')
    st['_last']={'boot':boot,'uptime':data['uptime_sec']}
    st['_last_sample_at']=now
    check_restart_recovery(db,st,data,now)
    if stale:
        return
    issue(db,st,'worker',data['worker_stalled'] if data['ble_ready'] else None,now,'El trabajador Bluetooth del ATOM no responde.')
    issue(db,st,'clock',not data['clock_valid'],now,'El reloj del ATOM no está sincronizado.')
    issue(db,st,'memory',data['heap_free']<60000,now,'Memoria libre del ATOM por debajo de 60 KB.')
    if data.get('ota'):
        for key in ('pv','ch','pv_err','ch_err','off','return'):
            issue(db,st,key,None,now,'')
        return
    for key,label in [('pv','Pez Verde'),('ch','Chihiros')]:
        lamp=data[key]
        active=lamp['expected'] and data['uptime_sec']>=180
        if not active:
            issue(db,st,key,None,now,'')
            issue(db,st,key+'_err',None,now,'')
        else:
            bad=(not lamp['ever_ok']) or lamp['ok_age_sec']>650
            issue(db,st,key,bad,now,label+' sin confirmación BLE reciente (tres comprobaciones).')
            issue(db,st,key+'_err',lamp['consecutive_errors']>=3,now,label+' acumula tres o más errores BLE consecutivos.')
    issue(db,st,'off',bool(data.get('off_pending')) and int(data.get('off_failures',0))>=3,now,'POWER OFF Pez Verde pendiente con al menos tres fallos de envío.',urgent=True)
    issue(db,st,'return',bool(data.get('ch_return_failed')),now,'No se ha completado el retorno de Chihiros a My Chihiros.',urgent=True)


def check_daily(db, now):
    local=datetime.datetime.fromtimestamp(now)
    day=local.date();yesterday=day-datetime.timedelta(days=1)
    if local.hour<9:return
    key=yesterday.isoformat()
    if read_meta(db,'last_daily','')==key:return
    start=int(time.mktime((yesterday.year,yesterday.month,yesterday.day,0,0,0,0,0,-1)))
    end=int(time.mktime((day.year,day.month,day.day,0,0,0,0,0,-1)))
    rows=db.execute('SELECT ts,ok FROM samples WHERE ts>=? AND ts<? ORDER BY ts',(start-POLL,end)).fetchall()
    inside=[r for r in rows if r[0]>=start]
    events=db.execute('SELECT ts,kind,message FROM events WHERE ts>=? AND ts<? ORDER BY ts',(start,end)).fetchall()
    count=lambda k:sum(e[1]==k for e in events)
    gaps=[];cursor=start;unreachable=0
    for i,(ts,good) in enumerate(rows):
        following=rows[i+1][0] if i+1<len(rows) else end
        span=following-ts if 0<=following-ts<=2*POLL else POLL
        a,b=max(start,ts,cursor),min(end,ts+span)
        if b<=a:continue
        if a>cursor:gaps.append((cursor,a))
        if not good:unreachable+=b-a
        cursor=b
    if cursor<end:gaps.append((cursor,end))
    missing=sum(b-a for a,b in gaps)
    message=('📋 CIELO · RESUMEN DIARIO {}\nConsultas: {} / unas {} previstas. Con acceso: {}. '
             'Reinicios detectados: {}. Recuperaciones: {}. Incidencias: {}.\n').format(
        key,len(inside),(end-start)//POLL,sum(r[1] for r in inside),count('reinicio'),count('recuperacion'),count('alerta'))
    if not inside:message+='SIN DATOS: ese día no estuvo observado; no equivale a un día sin fallos.\n'
    else:message+='Sin observador: {} min. Sin acceso durante consultas: {} min.\n'.format(missing//60,unreachable//60)
    for a,b in gaps[:6]:message+='Sin datos {}–{}.\n'.format(datetime.datetime.fromtimestamp(a).strftime('%H:%M'),datetime.datetime.fromtimestamp(b).strftime('%H:%M'))
    for ts,kind,text in [e for e in events if e[1] in ('reinicio','reinicios_sin_detalle')][-6:]:
        message+=datetime.datetime.fromtimestamp(ts).strftime('%H:%M')+' '+text+'\n'
    message+='GATT no mide la luz física.'
    outbox(db,'daily',message,now);meta(db,'last_daily',key);log('Resumen diario preparado')


def handle_stop(*args):
    global STOP
    STOP=True


def run():
    global STOP
    os.umask(0o077)
    os.environ['TZ']='CET-1CEST,M3.5.0,M10.5.0/3';time.tzset()
    if not CONFIG.exists():
        print('Primero ejecuta INSTALAR.command')
        return 1
    cfg=json.loads(CONFIG.read_text(encoding='utf-8'))
    make_db()
    lock_file=open(str(APP_DIR / "vigilancia.lock"),"a")
    try:
        fcntl.flock(lock_file.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
    except (IOError, OSError):
        log("Ya hay otro vigilante ejecutándose; no se inicia una copia duplicada")
        return 0
    try:
        signal.signal(signal.SIGTERM,handle_stop)
        signal.signal(signal.SIGINT,handle_stop)
    except ValueError:
        pass
    db=sqlite3.connect(str(DB_FILE),timeout=15)
    log('Vigilancia '+VERSION+' iniciada: sólo lectura cada 60 segundos')
    meta(db,'runtime_version',VERSION);meta(db,'runtime_started_at',int(time.time()));db.commit()
    start=0
    while not STOP:
        cycle_started=time.monotonic()
        now=int(time.time())
        try:
            data=get_atom(cfg['atom_url'])
            error=None
        except Exception as exc:
            data=None
            error=type(exc).__name__
            log('ATOM no disponible para consulta; '+error)
        try:
            st=read_meta(db,'conditions',{})
            last=read_meta(db,'last_poll_at',None)
            if last is not None and now-last>3*POLL:
                record_event(db,'observer-gap:'+str(last),'sin_datos','El iMac vuelve tras {} min sin consultas. Estado de las luces desconocido.'.format((now-last-POLL)//60),now)
            meta(db,'last_poll_at',now)
            record_sample(db,now,data,error)
            evaluate(db,st,data,now)
            meta(db,'conditions',st)
            check_daily(db,now)
            db.execute('DELETE FROM samples WHERE ts<?',(now-30*86400,))
            db.execute('DELETE FROM events WHERE ts<?',(now-30*86400,))
            db.commit()
            flush_outbox(db,cfg,now)
            db.commit()
            if now-start>=3600:
                log('Vigilancia operativa; ATOM '+('accesible' if data else 'sin acceso'))
                start=now
        except Exception as exc:
            try: db.rollback()
            except Exception: pass
            log('Error local del supervisor: '+type(exc).__name__)
        # Evita bucle ocupado, incluso tras error de red.
        deadline=cycle_started+POLL
        while not STOP and time.monotonic()<deadline:
            time.sleep(min(1,max(0,deadline-time.monotonic())))
    db.close()
    lock_file.close()
    log('Vigilancia detenida')
    return 0


def find_private_chat(token):
    print('\nAbre Telegram en tu iPhone, entra en el bot CIELO y envíale este mensaje exacto:')
    print('    /cielo_conectar')
    input('Cuando lo hayas enviado, pulsa INTRO aquí ... ')
    for attempt in range(3):
        result=telegram(token,'getUpdates',{'limit':100,'timeout':0})
        matches=[]
        for entry in result:
            m=entry.get('message') or {}
            chat=m.get('chat') or {}
            if (chat.get('type')=='private' and str(m.get('text','')).strip().startswith('/cielo_conectar')
                    and type(chat.get('id')) is int):
                matches.append((m.get('date',0),chat['id']))
        if matches:
            matches.sort()
            return str(matches[-1][1])
        if attempt==0:
            print('Todavía no se ve el mensaje. Comprueba que lo has enviado al bot correcto.')
            input('Vuelve a enviar /cielo_conectar y pulsa INTRO ... ')
        else:
            time.sleep(3)
    raise RuntimeError('Telegram no devuelve el chat privado; revisa otros observadores o webhook del bot')


def find_atom():
    print('Comprobando la API de lectura de tu RC5 SIMPLE1 en la Wi-Fi doméstica...')
    candidates=['http://iwagumi-cielo96.local','http://192.168.0.25']
    for url in candidates:
        try:
            d=get_atom(url)
            print('ATOM encontrado: {} (firmware {})'.format(url,d.get('firmware','desconocido')))
            return url,d
        except Exception:
            continue
    print('No se localizó CIELO automáticamente. Escribe la IP que muestra su interfaz; ejemplo 192.168.0.25')
    address=input('IP del ATOM (sin http): ').strip()
    if not valid_local_host(address):
        raise ValueError('IP no local')
    url='http://'+address
    d=get_atom(url)
    print('Conexión correcta a {}'.format(d.get('firmware','CIELO')))
    return url,d


def setup():
    if CONFIG.exists():return update_installation()
    if sys.version_info<(3,9):
        print('Este instalador requiere Python 3.9 o posterior. Versión detectada: '+sys.version.split()[0])
        return 1
    if sys.platform!='darwin':
        print('Instalador preparado para macOS; este equipo es '+sys.platform)
        return 1
    print('IWAGUMI CIELO - VIGILANCIA TELEGRAM PARA IMAC')
    print('Python: {}'.format(sys.version.split()[0]))
    print('Solo consulta el ATOM; no toca firmware, curvas ni luces.\n')
    try:
        url,data=find_atom()
    except Exception as exc:
        print('No se puede iniciar la vigilancia: la API de CIELO no responde ('+type(exc).__name__+').')
        print('Comprueba que iMac y ATOM están en la misma red y que RC5 SIMPLE1 responde en Safari.')
        return 1
    print('\nComprobando HTTPS de Telegram sin utilizar tu token...')
    if not check_telegram_https():
        print('No introduzcas el token mientras falle la red o el certificado.')
        print('Puedes ejecutar PROBAR_TELEGRAM.command para ver el diagnostico seguro.')
        return 1
    print('\nTOKEN: es el codigo largo que da @BotFather -> /mybots -> CIELO -> API Token.')
    print('No es la contrasena de CIELO, ni el nombre del bot, ni el texto /start.')
    print('Tiene forma 123456789:ABC... (numeros, dos puntos y una larga cadena).')
    print('Pulsa CMD+V para pegarlo cuando salga TOKEN; el terminal NO mostrara caracteres.')
    print('Despues pulsa INTRO. Nunca compartas el token por chat o capturas.')
    token=getpass.getpass('TOKEN del bot CIELO (pegado invisible): ').strip()
    if not re.fullmatch(r'[0-9]{5,20}:[A-Za-z0-9_-]{20,128}', token):
        print('NO VALIDO: lo que has pegado no tiene el formato de token de BotFather.')
        print('En BotFather abre /mybots > CIELO > API Token > Copy y vuelve a ejecutar.')
        print('Si no ves a CIELO en BotFather, revisa la cuenta que lo creo.')
        print('No se ha guardado ningun dato ni instalado el vigilante.')
        return 1
    try:
        bot=telegram(token,'getMe')
        print('Bot validado en Telegram: @'+str(bot.get('username','bot')))
        chat=find_private_chat(token)
        cfg={'atom_url':url,'telegram_token':token,'chat_id':chat,'poll_seconds':60,'created_at':int(time.time())}
        send_message(cfg,'IWAGUMI CIELO - Prueba de Telegram correcta.\nEl iMac ha localizado {}.\nEsta prueba no modifica las luces.'.format(data.get('firmware','RC5')))
    except urllib.error.HTTPError as exc:
        if exc.code==401:
            print('Telegram responde 401: el token no esta autorizado. Copia de nuevo el API Token de BotFather.')
        elif exc.code==409:
            print('Telegram responde 409: otro servicio del bot esta consultando los mensajes. Revisa el bot.')
        else:
            print('Telegram responde HTTP {}. No se ha instalado el vigilante.'.format(exc.code))
        return 1
    except (ssl.SSLError, urllib.error.URLError) as exc:
        print('No se pudo contactar Telegram: {}'.format(classify_telegram_failure(exc)))
        print('No se ha guardado el token ni instalado el vigilante. No desactives TLS.')
        return 1
    except ValueError as exc:
        print('Telegram: {}'.format(classify_telegram_failure(exc)))
        return 1
    except RuntimeError:
        print('No he podido localizar la conversacion privada. Envia /cielo_conectar al bot desde Telegram.')
        print('Si el bot tiene un webhook o un observador anterior, habra que comprobarlo.')
        return 1
    except Exception as exc:
        print('La prueba no se ha completado: {}. No se ha instalado nada.'.format(type(exc).__name__))
        return 1
    make_private()
    safe_write(CONFIG,cfg)
    shutil.copy2(str(pathlib.Path(__file__).resolve()),str(PY_SCRIPT))
    shutil.copy2(str(pathlib.Path(__file__).resolve().parent / TRUST_NAME),
                 str(APP_DIR / TRUST_NAME))
    os.chmod(str(APP_DIR / TRUST_NAME), 0o600)
    os.chmod(str(PY_SCRIPT),0o700)
    make_db()
    AGENT.parent.mkdir(parents=True,exist_ok=True)
    python=str(pathlib.Path(sys.executable).resolve())
    params=[python,str(PY_SCRIPT),'run']
    if pathlib.Path('/usr/bin/caffeinate').exists():
        params=['/usr/bin/caffeinate','-i']+params
    plist={'Label':LABEL,'ProgramArguments':params,'WorkingDirectory':str(APP_DIR),
           'RunAtLoad':True,'KeepAlive':True,'ThrottleInterval':30,
        'EnvironmentVariables':{'PYTHONIOENCODING':'utf-8','TZ':'CET-1CEST,M3.5.0,M10.5.0/3'},
           'StandardOutPath':str(LOG_FILE),'StandardErrorPath':str(LOG_FILE)}
    if AGENT.exists():
        subprocess.run(['/bin/launchctl','unload',str(AGENT)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    with AGENT.open('wb') as f: plistlib.dump(plist,f)
    os.chmod(str(AGENT),0o600)
    rc=subprocess.run(['/bin/launchctl','load','-w',str(AGENT)],stdout=subprocess.PIPE,stderr=subprocess.PIPE).returncode
    print('\nPrueba de Telegram enviada a tu iPhone. Comprueba el chat CIELO.')
    if rc:
        print('El monitor está configurado, pero macOS no inició el servicio de arranque.')
        print('Puedes ejecutarlo manualmente: python3 "{}" run'.format(PY_SCRIPT))
        return 1
    print('VIGILANCIA ACTIVADA: servicio automático al iniciar sesión, cada 60 segundos.')
    print('El monitor permanece activo mientras el iMac esté encendido y en sesión, conectado a Internet.')
    print('El token se conserva en un archivo privado (0600) del usuario del Mac.')
    print('Para consultar: bash CONSULTAR_ESTADO.command (desde la carpeta descargada).')
    return 0


def status():
    print('IWAGUMI CIELO - ESTADO DEL SUPERVISOR '+VERSION)
    print('Configuración:', 'presente' if CONFIG.exists() else 'no instalada')
    print('Arranque automático:', 'configurado' if AGENT.exists() else 'no configurado')
    if DB_FILE.exists():
        db=sqlite3.connect(str(DB_FILE))
        try:
            row=db.execute('SELECT ts,ok,firmware FROM samples ORDER BY ts DESC LIMIT 1').fetchone()
            if row:
                print('Última muestra:',datetime.datetime.fromtimestamp(row[0]).strftime('%Y-%m-%d %H:%M:%S'))
                print('ATOM:', 'accesible' if row[1] else 'sin respuesta')
                print('Firmware:',row[2] or 'no disponible')
            print('Versión del último proceso:',read_meta(db,'runtime_version','V3 / aún sin muestra V4'))
            n=db.execute('SELECT COUNT(*) FROM outbox').fetchone()[0]
            print('Avisos pendientes:',n)
        finally:
            db.close()
    if LOG_FILE.exists():
        print('\nÚltimas líneas del registro (sin credenciales):')
        for line in LOG_FILE.read_text(encoding='utf-8',errors='replace').splitlines()[-12:]:
            print(line)
    return 0


def stop_service():
    if AGENT.exists():
        subprocess.run(['/bin/launchctl','unload','-w',str(AGENT)])
        print('Servicio detenido. Configuración e historial conservados.')
    else:
        print('No hay servicio automático instalado.')
    return 0


if __name__=='__main__':
    args=sys.argv[1:]
    operation=args[0] if args else 'status'
    try:
        if operation=='setup': sys.exit(setup())
        if operation=='update': sys.exit(update_installation())
        if operation=='rollback': sys.exit(rollback_installation())
        if operation=='run': sys.exit(run())
        if operation=='status': sys.exit(status())
        if operation=='stop': sys.exit(stop_service())
        if operation=='diagnose': sys.exit(diagnosis())
        print('Uso: python3 cielo_observer.py setup|run|status|stop|diagnose')
        sys.exit(2)
    except Exception as exc:
        print('Operación no completada: '+type(exc).__name__+'. La configuración privada no se muestra.')
        sys.exit(1)
    except KeyboardInterrupt:
        print('\nInterrumpido')
        sys.exit(1)
