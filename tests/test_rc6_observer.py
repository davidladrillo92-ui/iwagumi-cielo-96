"""Regression scenarios from boots 109→110→111; no Telegram/ATOM access."""
import ast
import copy
import datetime as dt
from pathlib import Path
import sys
import tempfile
import unittest
from zoneinfo import ZoneInfo
sys.path.insert(0,str(Path('monitor').resolve()))
sys.path.insert(0,str(Path('tests').resolve()))
from cielo_monitor.engine import Engine
from cielo_monitor.store import Store
from cielo_monitor.transport import validate, PollError
from cielo_monitor.coverage import coverage
from cielo_monitor.providers import deliver_one, DeliveryError
from test_b89_observer import sample

def extended(boot, device=0, stamp=0):
    d=sample();d.update(boot_id=boot,uptime_sec=240,recovery_schema=1,
        boot_marked_healthy=True,reset_reason=3,recovered_boot=True,restart_total=boot-1,
        last_stall_phase=4,last_stall_device=device,last_stall_ms=120001)
    d['recent_boots']=[dict(boot_id=boot,previous_boot_id=boot-1,epoch=stamp,
        reset_reason=3,ble_recovery=True,phase=4,device=device,stall_ms=120001,
        stall_epoch=stamp-1 if stamp else 0,outcome=1,recovered_epoch=stamp+180 if stamp else 0)]
    return d

class RecoveryTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.store=Store(Path(self.temp.name)/'history.sqlite')
        self.engine=Engine(self.store,['telegram']);self.now=1792710000
    def tearDown(self):self.temp.cleanup()
    def tick(self,d,seconds=60):
        self.now+=seconds;self.engine.tick(self.now,d)
    def events(self,kind):
        return [e for e in self.store.history(7,self.now)['events'] if e['kind']==kind]
    def test_first_reboot_and_second_outside_hour_are_each_notified(self):
        d=sample();d['boot_id']=109;self.tick(d)
        self.assertFalse(self.events('restart'))
        self.tick(extended(110,1))
        self.assertEqual(len(self.events('restart')),1)
        self.assertIn('Chihiros',self.events('restart')[0]['text'])
        self.assertEqual(len(self.events('recovery')),1)
        self.engine=Engine(Store(self.store.path),['telegram']) # iMac process restart preserves dedup
        self.tick(extended(110,1))
        self.assertEqual(len(self.events('restart')),1)
        self.tick(extended(111,0),4000)
        self.assertEqual(len(self.events('restart')),2)
        self.assertIn('Pez Verde',self.events('restart')[0]['text'])
        with self.store.db() as c:
            keys=[r[0] for r in c.execute("SELECT key FROM outbox WHERE provider='telegram'")]
        self.assertIn('restart:110',keys);self.assertIn('restart:111',keys)
        self.assertIn('restart-recovered:110',keys);self.assertIn('restart-recovered:111',keys)
    def test_legacy_simple1_already_detects_a_reboot_without_firmware_update(self):
        d=sample();self.tick(d);d['boot_id']+=1;d['uptime_sec']=300;self.tick(d)
        self.assertEqual(len(self.events('restart')),1)
        self.assertIn('causa no disponible',self.events('restart')[0]['text'])
    def test_multiple_restarts_between_polls_recovered_from_closed_history(self):
        self.tick(extended(109))
        d=extended(111);d['recent_boots'].insert(0,extended(110,1)['recent_boots'][0])
        self.tick(d)
        self.assertEqual(len(self.events('restart')),2)
        self.tick(d);self.assertEqual(len(self.events('restart')),2)
    def test_uptime_backwards_without_id_change_and_rollover(self):
        d=sample();d['uptime_sec']=8000;self.tick(d)
        d['uptime_sec']=200;self.tick(d)
        self.assertEqual(len(self.events('restart')),1)
        self.assertIn('contador inconsistente',self.events('restart')[0]['text'])
        d['uptime_sec']=4294960;self.tick(d);d['uptime_sec']=10;self.tick(d)
        self.assertEqual(len(self.events('restart')),1)
    def test_loss_of_data_cannot_announce_boot_recovery(self):
        self.tick(extended(109));d=extended(110);d['off_pending']=True;self.tick(d)
        self.tick(None);self.assertFalse(self.events('recovery'))
        d['off_pending']=False;self.tick(d);self.assertEqual(len(self.events('recovery')),1)
    def test_restart_outbox_keeps_distinct_boots_and_retries(self):
        self.tick(extended(109));self.tick(extended(110));self.tick(extended(111))
        class Sender:
            def __init__(self):self.fail=True;self.sent=[]
            def send(self,payload):
                if self.fail:raise DeliveryError(90)
                self.sent.append(payload)
        sender=Sender();self.assertTrue(deliver_one(self.store,{'telegram':sender},self.now))
        with self.store.db() as c:
            self.assertTrue(c.execute('SELECT COUNT(*) FROM outbox WHERE attempts=1').fetchone()[0])
        sender.fail=False
        for _ in range(20):
            if not deliver_one(self.store,{'telegram':sender},self.now+3600):break
        self.assertEqual(sum(p['kind']=='restart' for p in sender.sent),2)
    def test_optional_extension_closed_and_backward_compatible(self):
        self.assertNotIn('recent_boots',validate(sample()))
        d=extended(110);d['recent_boots'][0]['private_token']='not retained'
        self.assertNotIn('private_token',validate(d)['recent_boots'][0])
        for mutate in (lambda d:d['recent_boots'].append(d['recent_boots'][0]),
                       lambda d:d.update(recovery_schema=2),
                       lambda d:d['recent_boots'][0].update(phase=99),
                       lambda d:d['recent_boots'][0].update(outcome=True)):
            d=extended(110);mutate(d)
            with self.assertRaises(PollError):validate(d)
    def test_coverage_marks_gaps_and_distinguishes_unreachable(self):
        rows=[{'ts':60,'data':'yes'},{'ts':120,'data':None},{'ts':600,'data':'yes'}]
        c=coverage(rows,0,720,60)
        self.assertEqual(c['gaps'],[[0,60],[180,600]])
        self.assertEqual(c['unreachable_seconds'],60)
        self.assertEqual(sum(c[k] for k in ('reachable_seconds','unreachable_seconds','no_observer_seconds')),720)
    def test_daily_no_observations_is_explicit_and_dst_has_25_hours(self):
        now=int(dt.datetime(2026,10,26,10,tzinfo=ZoneInfo('Europe/Madrid')).timestamp())
        self.engine.tick(now,sample());self.now=now
        text=self.events('summary')[0]['text']
        self.assertIn('SIN DATOS',text);self.assertIn('1500',text)
        self.engine.tick(now+60,sample());self.assertEqual(len(self.events('summary')),1)
    def test_python39_syntax_for_sierra(self):
        for p in Path('monitor/cielo_monitor').glob('*.py'):
            ast.parse(p.read_text(),feature_version=(3,9))

if __name__=='__main__':unittest.main()
