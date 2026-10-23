"""Execute the actual Sierra V3→V4 installer with launchctl/network simulated.
No live Telegram, services, credentials or ATOM are used.
"""
import ast
import copy
import datetime
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import plistlib
import sqlite3
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'monitor/macos_sierra'
spec=importlib.util.spec_from_file_location('sierra',SOURCE/'cielo_observer.py')
sierra=importlib.util.module_from_spec(spec);spec.loader.exec_module(sierra)
sys.path.insert(0,str(ROOT/'tests'))
from test_b89_observer import sample
from test_rc6_observer import extended

class SierraTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='Sierra con espacios ')
        root=Path(self.temp.name);app=root/'Library/Application Support/IWAGUMI CIELO Vigilancia'
        agent=root/'Library/LaunchAgents/es.iwagumi.cielo.vigilancia.plist'
        agent.parent.mkdir(parents=True)
        self.paths=dict(APP_DIR=app,CONFIG=app/'config.json',DB_FILE=app/'historial.sqlite',
            LOG_FILE=app/'vigilancia.log',AGENT=agent,PY_SCRIPT=app/'cielo_observer.py')
        self.patch=patch.multiple(sierra,**self.paths);self.patch.start()
        sierra.make_db()
        self.config={'atom_url':'http://192.168.0.25','telegram_token':'123456789:'+'A'*30,'chat_id':'12345678','created_at':100}
        sierra.safe_write(sierra.CONFIG,self.config)
        sierra.PY_SCRIPT.write_text('# Vigilante V3 original\n')
        (app/sierra.TRUST_NAME).write_bytes((SOURCE/sierra.TRUST_NAME).read_bytes())
        with agent.open('wb') as f:plistlib.dump(sierra.agent_plist(sys.executable),f)
        with sqlite3.connect(str(sierra.DB_FILE)) as db:
            sierra.record_sample(db,100,sample(),None)
            sierra.meta(db,'conditions',{'_last':{'boot':109,'uptime':999}})
            sierra.outbox(db,'previous','Aviso previo que debe conservarse',100)
        self.original={str(p):p.read_bytes() for p in (sierra.CONFIG,sierra.PY_SCRIPT,sierra.AGENT,sierra.DB_FILE)}
    def tearDown(self):self.patch.stop();self.temp.cleanup()
    def test_update_preserves_v3_config_database_and_queue(self):
        calls=[]
        with patch.object(sierra.sys,'platform','darwin'),patch.object(sierra,'launch',side_effect=lambda args:calls.append(args) or 0),patch.object(sierra,'telegram',side_effect=AssertionError('No Telegram call')),patch('sys.stdout',new=io.StringIO()):
            self.assertEqual(sierra.setup(),0) # existing config follows update, not pairing
        self.assertEqual(sierra.CONFIG.read_bytes(),self.original[str(sierra.CONFIG)])
        self.assertEqual(sierra.DB_FILE.read_bytes(),self.original[str(sierra.DB_FILE)])
        self.assertIn('4.0.0-SIERRA',sierra.PY_SCRIPT.read_text())
        with sqlite3.connect(str(sierra.DB_FILE)) as db:
            self.assertEqual(db.execute('SELECT COUNT(*) FROM samples').fetchone()[0],1)
            self.assertEqual(db.execute('SELECT msg FROM outbox WHERE key="previous"').fetchone()[0],'Aviso previo que debe conservarse')
        self.assertEqual(calls[0][0],'unload');self.assertEqual(calls[-1][0],'load')
        backups=list((sierra.APP_DIR/'backups').glob('*'));self.assertEqual(len(backups),1)
        self.assertEqual((backups[0]/'historial.sqlite').stat().st_mode&0o777,0o600)
        self.assertEqual((backups[0]/'config.json').read_bytes(),self.original[str(sierra.CONFIG)])
        # Explicit rollback keeps the current token/history while restoring code.
        with patch.object(sierra.sys,'platform','darwin'),patch.object(sierra,'launch',return_value=0),patch('sys.stdout',new=io.StringIO()):
            self.assertEqual(sierra.rollback_installation(),0)
        self.assertEqual(sierra.PY_SCRIPT.read_bytes(),self.original[str(sierra.PY_SCRIPT)])
        self.assertEqual(sierra.CONFIG.read_bytes(),self.original[str(sierra.CONFIG)])
        self.assertEqual(sierra.DB_FILE.read_bytes(),self.original[str(sierra.DB_FILE)])
    def test_launch_failure_restores_code_and_service(self):
        loads=[]
        def launch(args):
            if args[0]=='load':loads.append(args);return 1 if len(loads)==1 else 0
            return 0
        with patch.object(sierra.sys,'platform','darwin'),patch.object(sierra,'launch',side_effect=launch),patch('sys.stdout',new=io.StringIO()):
            with self.assertRaises(RuntimeError):sierra.update_installation()
        for p in (sierra.CONFIG,sierra.PY_SCRIPT,sierra.AGENT,sierra.DB_FILE):
            self.assertEqual(p.read_bytes(),self.original[str(p)])
        self.assertEqual(len(loads),2)
    def test_incompatible_database_is_rejected_before_stopping_service(self):
        with sqlite3.connect(str(sierra.DB_FILE)) as db:db.execute('ALTER TABLE samples RENAME TO incompatible')
        with patch.object(sierra.sys,'platform','darwin'),patch.object(sierra,'launch') as launch:
            with self.assertRaises(ValueError):sierra.update_installation()
            launch.assert_not_called()
    def test_corrupt_ca_is_rejected_before_stopping_service(self):
        with patch.object(sierra,'TRUST_EXPECTED_SHA256','0'*64),patch.object(sierra.sys,'platform','darwin'),patch.object(sierra,'launch') as launch:
            with self.assertRaises(ValueError):sierra.update_installation()
            launch.assert_not_called()
    def test_certificates_verified_without_system_ca(self):
        ctx=sierra.telegram_tls_context()
        import ssl
        self.assertTrue(ctx.check_hostname);self.assertEqual(ctx.verify_mode,ssl.CERT_REQUIRED)
        self.assertGreater(len(ctx.get_ca_certs()),100)
        self.assertGreaterEqual(ctx.minimum_version,ssl.TLSVersion.TLSv1_2)
    def test_first_reboot_and_second_between_polls_survive_imac_restart(self):
        with sqlite3.connect(str(sierra.DB_FILE)) as db:
            st=sierra.read_meta(db,'conditions',{})
            sierra.evaluate(db,st,extended(110,1),1000)
            sierra.meta(db,'conditions',st);db.commit()
            st=sierra.read_meta(db,'conditions',{}) # new process reads existing V3 meta format
            sierra.evaluate(db,st,extended(110,1),1060)
            sierra.evaluate(db,st,extended(111,0),5000)
            messages=db.execute("SELECT key,message FROM events WHERE kind='reinicio'").fetchall()
            self.assertEqual(len(messages),2)
            self.assertIn('Chihiros',messages[0][1]);self.assertIn('Pez Verde',messages[1][1])
            keys={r[0] for r in db.execute('SELECT key FROM outbox')}
            for key in ('restart:110','restart:111','restart-recovered:110','restart-recovered:111'):self.assertIn(key,keys)
    def test_baseline_unknown_is_not_a_restart(self):
        with sqlite3.connect(str(sierra.DB_FILE)) as db:
            sierra.evaluate(db,{},extended(111),1000)
            self.assertEqual(db.execute("SELECT COUNT(*) FROM events WHERE kind='reinicio'").fetchone()[0],0)
    def test_daily_without_data_and_madrid_dst(self):
        prior=os.environ.get('TZ');os.environ['TZ']='CET-1CEST,M3.5.0,M10.5.0/3';sierra.time.tzset()
        try:
            now=int(sierra.time.mktime((2026,10,26,10,0,0,0,0,-1)))
            with sqlite3.connect(str(sierra.DB_FILE)) as db:
                sierra.check_daily(db,now)
                msg=db.execute('SELECT msg FROM outbox WHERE key="daily"').fetchone()[0]
                self.assertIn('SIN DATOS',msg);self.assertIn('1500',msg)
        finally:
            if prior is None:os.environ.pop('TZ',None)
            else:os.environ['TZ']=prior
            sierra.time.tzset()
    def test_shells_python39_and_launchagent_contract(self):
        ast.parse((SOURCE/'cielo_observer.py').read_text(),feature_version=(3,9))
        for path in list(SOURCE.glob('*.command'))+list(SOURCE.glob('*.sh')):
            subprocess.run(['/bin/bash','-n',str(path)],check=True)
        agent=sierra.agent_plist('/Library/Frameworks/Python.framework/Versions/3.9/bin/python3.9')
        self.assertEqual(agent['Label'],'es.iwagumi.cielo.vigilancia')
        self.assertTrue(agent['RunAtLoad'] and agent['KeepAlive'])
        self.assertNotEqual(agent['StandardOutPath'],str(sierra.LOG_FILE))
        self.assertEqual(sierra.APP_DIR.name,'IWAGUMI CIELO Vigilancia')

if __name__=='__main__':unittest.main()
