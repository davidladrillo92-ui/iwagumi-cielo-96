import copy
import datetime as dt
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import socket
import ssl
import subprocess
import sys
import tempfile
import threading
import unittest
from http.server import HTTPServer, BaseHTTPRequestHandler
from unittest.mock import patch
from zoneinfo import ZoneInfo
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'monitor'))
from cielo_monitor.transport import AtomClient, PollError, validate
from cielo_monitor.store import Store
from cielo_monitor.engine import Engine
from cielo_monitor.providers import DeliveryError, deliver_one, Telegram
from cielo_monitor.server import serve

NOW=1792620000

def sample():
 return dict(schema='cielo-monitor-v1',firmware='11.1.0-RC5-B1-B9-LAN1',boot_id=1,uptime_sec=1000,epoch=NOW,
 clock_valid=True,ble_ready=True,worker_stalled=False,ota=False,heap_free=120000,heap_min=80000,
 largest_block=100000,ble_stack_free_bytes=4000,queue_depth=0,worker_queue_depth=0,ch_return_failed=False,off_pending=False,off_failures=0,
 profile=dict(generation=1,pv_points=34,ch_points=31),
 pv=dict(expected=True,ever_ok=True,ok_age_sec=5,consecutive_errors=0,operations=10,errors=0),
 ch=dict(expected=True,ever_ok=True,ok_age_sec=5,consecutive_errors=0,operations=10,errors=0),optical_feedback=None)

class ObserverTests(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory();self.store=Store(Path(self.temp.name)/'history.sqlite');self.engine=Engine(self.store,['telegram']);self.now=NOW
 def tearDown(self):self.temp.cleanup()
 def tick(self,data=None,error='transport'):
  self.now+=60
  if data is not None:
   data=copy.deepcopy(data);data['uptime_sec']+=self.now-NOW
  self.engine.tick(self.now,data,error if data is None else None)
 def events(self,key=None,kind=None):
  with self.store.db() as c:
   rows=[dict(r) for r in c.execute('SELECT * FROM events')]
  return [r for r in rows if (key is None or r['key']==key) and (kind is None or r['kind']==kind)]
 def test_absence_and_recovery_survive_restart(self):
  self.tick(sample());self.tick();self.tick();self.assertEqual(len(self.events('atom','alert')),0)
  self.engine=Engine(self.store,['telegram']);self.tick();self.assertEqual(len(self.events('atom','alert')),1)
  for _ in range(10):self.tick()
  self.assertEqual(len(self.events('atom','alert')),1)
  self.tick(sample());self.assertEqual(len(self.events('atom','recovery')),0)
  self.tick(sample());self.assertEqual(len(self.events('atom','recovery')),1)
 def test_ceded_lamp_is_unknown_not_failed_or_recovered(self):
  d=sample();d['pv']['consecutive_errors']=3
  for _ in range(3):self.tick(d)
  self.assertEqual(len(self.events('pv_errors','alert')),1)
  d['pv']['expected']=False
  for _ in range(5):self.tick(d)
  self.assertEqual(len(self.events('pv_errors','recovery')),0)
 def test_old_error_counters_do_not_alert(self):
  d=sample();d['pv']['errors']=50
  for _ in range(8):self.tick(d)
  self.assertEqual(len(self.events('pv_errors','alert')),0)
 def test_repeated_errors_window_and_restart(self):
  d=sample();self.tick(d)
  for count in (3,4,5):d['pv']['errors']=count;self.tick(d)
  self.assertEqual(len(self.events('pv_errors','alert')),1)
  d['boot_id']=2;d['pv']['errors']=0
  for _ in range(2):self.tick(d)
  self.assertEqual(len(self.events('pv_errors','recovery')),1)
 def test_stale_lamp_and_off_pending(self):
  d=sample();d['pv']['ok_age_sec']=700;d['off_pending']=True;d['off_failures']=3
  self.tick(d);self.assertEqual(len(self.events('off','alert')),1)
  self.tick(d);self.tick(d);self.assertEqual(len(self.events('pv','alert')),1)
 def test_missing_data_cannot_recover_ble(self):
  d=sample();d['worker_stalled']=True
  for _ in range(3):self.tick(d)
  for _ in range(3):self.tick()
  self.assertEqual(len(self.events('worker','recovery')),0)
 def test_retention_and_history(self):
  self.tick(sample())
  with self.store.db() as c:c.execute('INSERT INTO samples VALUES(?,?,?)',(self.now-31*86400,None,'transport'))
  self.store.prune(self.now)
  self.assertEqual(sum(x['samples'] for x in self.store.history(30,self.now)['hours']),1)
  self.assertRaises(ValueError,self.store.history,365)
 def test_daily_once_madrid_dst(self):
  zone=ZoneInfo('Europe/Madrid');now=int(dt.datetime(2026,10,26,10,tzinfo=zone).timestamp())
  self.engine.tick(now,sample());self.engine.tick(now+3600,sample())
  summaries=[r for r in self.events('daily') if '2026-10-25' in r['text']]
  self.assertEqual(len(summaries),1);self.assertIn('1500 previstas',summaries[0]['text'])
 def test_outbox_coalesces_failure_and_recovery(self):
  for _ in range(3):self.tick()
  self.tick(sample());self.tick(sample())
  with self.store.db() as c:r=c.execute("SELECT payload FROM outbox WHERE key='atom'").fetchall()
  self.assertEqual(len(r),1);self.assertEqual(json.loads(r[0][0])['kind'],'recovery')
 def test_delivery_backoff_and_retry_after(self):
  with self.store.db() as c:Store.enqueue(c,self.now,'fake','x',{'text':'a'})
  class Fail:
   def send(self,p):raise DeliveryError(123)
  deliver_one(self.store,{'fake':Fail()},self.now)
  with self.store.db() as c:r=c.execute('SELECT * FROM outbox').fetchone()
  self.assertEqual(r['attempts'],1);self.assertEqual(r['due'],self.now+123)
  class Ok:
   def send(self,p):pass
  deliver_one(self.store,{'fake':Ok()},self.now+123)
  with self.store.db() as c:self.assertEqual(c.execute('SELECT COUNT(*) FROM outbox').fetchone()[0],0)
 def test_deliver_does_not_delete_newer_state(self):
  with self.store.db() as c:Store.enqueue(c,self.now,'fake','x',{'text':'old'})
  outer=self
  class Provider:
   def send(self,p):
    with outer.store.db() as c:Store.enqueue(c,outer.now+1,'fake','x',{'text':'new'})
  deliver_one(self.store,{'fake':Provider()},self.now)
  with self.store.db() as c:self.assertIn('new',c.execute('SELECT payload FROM outbox').fetchone()[0])
 def test_bounded_outbox(self):
  with self.store.db() as c:
   for i in range(250):Store.enqueue(c,self.now,'fake',str(i),{})
   self.assertEqual(c.execute('SELECT COUNT(*) FROM outbox').fetchone()[0],200)
 def test_schema_closed_and_strict(self):
  d=sample();d['password']='DO_NOT_STORE'
  self.assertNotIn('password',validate(d))
  d['pv']['expected']='false';self.assertRaises(PollError,validate,d)
 def test_replay_detected(self):
  for i in range(8):self.engine.tick(self.now+i*60,sample())
  self.assertEqual(len(self.events('stale','alert')),1)
 def test_ch_return_failure_immediate(self):
  d=sample();d['ch_return_failed']=True;self.tick(d)
  self.assertEqual(len(self.events('ch_return','alert')),1)
 def test_restarts_are_not_normalized_away(self):
  d=sample();self.tick(d)
  for boot in (2,3,4,4,4):d['boot_id']=boot;self.tick(d)
  self.assertEqual(len(self.events('restarts','alert')),1)
 def test_flapping_does_not_spam_recoveries(self):
  for _ in range(3):self.tick()
  self.tick(sample());self.tick(sample())
  for _ in range(3):self.tick()
  self.tick(sample());self.tick(sample())
  with self.store.db() as c:
   r=c.execute("SELECT payload FROM outbox WHERE key='atom'").fetchone()
  # Segunda incidencia a <10 min: recuperación no reemplaza primera notificación.
  self.assertLess(json.loads(r[0])['time'],self.now)
 def test_mqtt_discovery_tls_and_no_command_channel(self):
  from cielo_monitor.providers import MQTT
  from unittest.mock import MagicMock
  client=MagicMock();client.publish.return_value.is_published.return_value=True
  with patch('paho.mqtt.client.Client',return_value=client):
   provider=MQTT({'host':'localhost','home_assistant':True},'')
   provider.ready.set();provider.send({'kind':'state','reachable':True,'sample':sample()})
   topics=[c.args[0] for c in client.publish.call_args_list]
   self.assertTrue(client.tls_set.called);self.assertTrue(client.will_set.called)
   self.assertIn('cielo/iwagumi96/state',topics)
   self.assertFalse(any('/set' in t for t in topics));self.assertEqual(len([t for t in topics if t.endswith('/config')]),2)
   provider.close()
 def test_telegram_mock_no_network(self):
  with patch('cielo_monitor.providers.https_request',return_value=b'{"ok":true}') as send:
   Telegram('test','chat').send({'text':'Prueba'})
   self.assertEqual(send.call_args.args[1]['text'],'Prueba')
 def test_dashboard_private_api(self):
  import urllib.request,urllib.error
  server=serve(self.store,'a'*32,0);thread=threading.Thread(target=server.serve_forever);thread.start()
  try:
   url='http://127.0.0.1:'+str(server.server_port)+'/api/history?days=7'
   with self.assertRaises(urllib.error.HTTPError) as e:urllib.request.urlopen(url)
   self.assertEqual(e.exception.code,401)
   req=urllib.request.Request(url,headers={'Authorization':'Bearer '+'a'*32})
   with urllib.request.urlopen(req) as r:self.assertEqual(json.load(r)['days'],7)
  finally:server.shutdown();server.server_close();thread.join()

class TLSTests(unittest.TestCase):
 def test_pin_before_credential_no_redirects_and_size_limit(self):
  from cryptography import x509
  from cryptography.x509.oid import NameOID
  from cryptography.hazmat.primitives import hashes,serialization
  from cryptography.hazmat.primitives.asymmetric import ec
  key=ec.generate_private_key(ec.SECP256R1());name=x509.Name([x509.NameAttribute(NameOID.COMMON_NAME,'localhost')])
  now=dt.datetime.now(dt.timezone.utc)
  cert=x509.CertificateBuilder().subject_name(name).issuer_name(name).public_key(key.public_key()).serial_number(1).not_valid_before(now-dt.timedelta(days=1)).not_valid_after(now+dt.timedelta(days=1)).sign(key,hashes.SHA256())
  requests=[];mode=['normal']
  class QuietServer(HTTPServer):
   def handle_error(self,*args):pass
  class Handler(BaseHTTPRequestHandler):
   def log_message(self,*a):pass
   def do_GET(self):
    requests.append(dict(self.headers));self.send_response(302 if mode[0]=='redirect' else 200);self.end_headers()
    body=b'x'*8193 if mode[0]=='large' else json.dumps(sample()).encode()
    try:self.wfile.write(body)
    except OSError:pass
  with tempfile.TemporaryDirectory() as t:
   certpath=Path(t)/'cert.pem';keypath=Path(t)/'key.pem'
   certpath.write_bytes(cert.public_bytes(serialization.Encoding.PEM));keypath.write_bytes(key.private_bytes(serialization.Encoding.PEM,serialization.PrivateFormat.PKCS8,serialization.NoEncryption()))
   server=QuietServer(('127.0.0.1',0),Handler);ctx=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);ctx.load_cert_chain(certpath,keypath);server.socket=ctx.wrap_socket(server.socket,server_side=True)
   thread=threading.Thread(target=server.serve_forever);thread.start()
   try:
    url='https://127.0.0.1:'+str(server.server_port);token='a'*64
    wrong=AtomClient(url,'0'*64,token)
    self.assertRaises(PollError,wrong.poll);self.assertEqual(requests,[])
    client=AtomClient(url,cert.fingerprint(hashes.SHA256()).hex(),token)
    self.assertEqual(client.poll()['profile']['ch_points'],31);self.assertEqual(requests[0]['X-Cielo-Monitor'],token)
    mode[0]='redirect';self.assertRaises(PollError,client.poll)
    mode[0]='large';self.assertRaises(PollError,client.poll)
   finally:server.shutdown();server.server_close();thread.join()

if __name__=='__main__':unittest.main()
