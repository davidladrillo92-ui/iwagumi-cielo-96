"""Real local HTTP observer requests without any credentials; no external services."""
from pathlib import Path
import sys,json,threading,tempfile,unittest,urllib.request
from http.server import HTTPServer,BaseHTTPRequestHandler
sys.path.insert(0,str(Path('monitor').resolve()))
from cielo_monitor.transport import LocalAtomClient,configured_client,PollError
from cielo_monitor.server import serve
from cielo_monitor.store import Store
sys.path.insert(0,str(Path('tests').resolve()))
from test_b89_observer import sample
class LanTests(unittest.TestCase):
 def test_client_reads_without_secret_and_rejects_redirects(self):
  requests=[];mode=[200]
  class Handler(BaseHTTPRequestHandler):
   def log_message(self,*args):pass
   def do_GET(self):
    requests.append((self.path,dict(self.headers)));self.send_response(mode[0]);self.send_header('Location','http://example.invalid');self.end_headers();self.wfile.write(json.dumps(sample()).encode())
  server=HTTPServer(('127.0.0.1',0),Handler);thread=threading.Thread(target=server.serve_forever);thread.start()
  try:
   client=configured_client({'atom_url':'http://127.0.0.1:'+str(server.server_port),'local_lan':True})
   self.assertEqual(client.poll()['profile']['pv_points'],34)
   self.assertEqual(requests[0][0],'/api/monitor')
   self.assertFalse(any('token' in h.lower() or 'authorization' in h.lower() or 'cielo' in h.lower() for h in requests[0][1]))
   mode[0]=302
   with self.assertRaises(PollError):client.poll()
   self.assertEqual(len(requests),2)
  finally:server.shutdown();server.server_close();thread.join()
 def test_remote_and_malformed_urls_rejected(self):
  for url in ['https://192.168.0.25','http://8.8.8.8','http://example.com','http://192.168.0.25/private','http://user:pass@192.168.0.25','http://192.168.0.25?token=x']:
   with self.assertRaises(ValueError):LocalAtomClient(url)
  LocalAtomClient('http://iwagumi-cielo96.local')
 def test_panel_without_password(self):
  with tempfile.TemporaryDirectory() as d:
   store=Store(str(Path(d)/'db.sqlite'));server=serve(store,port=0);thread=threading.Thread(target=server.serve_forever);thread.start()
   try:
    url='http://127.0.0.1:'+str(server.server_port)
    with urllib.request.urlopen(url+'/api/access') as r:self.assertFalse(json.load(r)['password_required'])
    with urllib.request.urlopen(url+'/api/history?days=7') as r:self.assertEqual(json.load(r)['days'],7)
   finally:server.shutdown();server.server_close();thread.join()
if __name__=='__main__':unittest.main()
