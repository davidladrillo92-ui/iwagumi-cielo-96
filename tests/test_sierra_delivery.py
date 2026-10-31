"""Regression: recoveries/daily reports must not overwrite undelivered events."""
import importlib.util,pathlib,tempfile,sqlite3,unittest,datetime
from unittest.mock import patch
path=pathlib.Path('monitor/macos_sierra/cielo_observer.py')
spec=importlib.util.spec_from_file_location('sierra_delivery',path);s=importlib.util.module_from_spec(spec);spec.loader.exec_module(s)


class DeliveryTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();root=pathlib.Path(self.temp.name)
        self.guard=patch.multiple(s,APP_DIR=root,DB_FILE=root/'history.sqlite',LOG_FILE=root/'log.txt');self.guard.start();s.make_db()
        self.db=sqlite3.connect(str(s.DB_FILE))

    def tearDown(self):self.db.close();self.guard.stop();self.temp.cleanup()

    def test_alert_and_recovery_survive_offline_restart(self):
        state={};s.issue(self.db,state,'pv',True,1000,'Pez Verde sin comunicacion',urgent=True)
        s.meta(self.db,'conditions',state);self.db.commit()
        self.db.close();self.db=sqlite3.connect(str(s.DB_FILE));state=s.read_meta(self.db,'conditions',{})
        s.issue(self.db,state,'pv',False,1060,'Pez Verde sin comunicacion')
        s.issue(self.db,state,'pv',False,1120,'Pez Verde sin comunicacion')
        rows=self.db.execute('SELECT msg FROM outbox ORDER BY due').fetchall()
        self.assertEqual(len(rows),2)
        self.assertIn('ALERTA',rows[0][0]);self.assertIn('RECUPERACIÓN',rows[1][0])
        with patch.object(s,'send_message') as send:
            s.flush_outbox(self.db,{},1200);s.flush_outbox(self.db,{},1200)
            self.assertEqual(send.call_count,2)
        self.assertEqual(self.db.execute('SELECT COUNT(*) FROM outbox').fetchone()[0],0)

    def test_duplicate_event_preserves_retry_state(self):
        s.outbox(self.db,'same-event','original',1000)
        self.db.execute('UPDATE outbox SET attempts=4,due=2000')
        s.outbox(self.db,'same-event','duplicate',1100)
        self.assertEqual(self.db.execute('SELECT msg,due,attempts FROM outbox').fetchone(),('original',2000,4))

    def test_daily_backlog_keeps_each_day(self):
        for day in [28,29]:
            now=int(datetime.datetime(2026,10,day,10).timestamp())
            s.check_daily(self.db,now);s.check_daily(self.db,now)
        rows=self.db.execute("SELECT key,msg FROM outbox").fetchall()
        self.assertEqual(len(rows),2);self.assertTrue(all('RESUMEN DIARIO' in r[1] for r in rows))

    def test_telegram_failure_does_not_delete_other_events(self):
        s.outbox(self.db,'first','alert',1000);s.outbox(self.db,'second','recovery',1100)
        with patch.object(s,'send_message',side_effect=OSError('PRIVATE_SECRET')):
            s.flush_outbox(self.db,{},1200)
        self.assertEqual(self.db.execute('SELECT COUNT(*) FROM outbox').fetchone()[0],2)
        self.assertEqual(self.db.execute('SELECT attempts FROM outbox WHERE key="first"').fetchone()[0],1)
        self.assertNotIn('PRIVATE_SECRET',s.LOG_FILE.read_text())


if __name__=='__main__':unittest.main()
