"""Local reversible observer update: never touches credentials or the live DB."""
import fcntl
import importlib.util
from pathlib import Path
import sqlite3
import tempfile
import unittest
spec=importlib.util.spec_from_file_location('updater','tools/update_observer.py')
updater=importlib.util.module_from_spec(spec);spec.loader.exec_module(updater)

class UpdateTests(unittest.TestCase):
    def test_identified_package_backup_and_existing_files_unchanged(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);pkg=root/'cielo_monitor';pkg.mkdir()
            for file in ('engine.py','__main__.py'):(pkg/file).write_text('# old\n')
            cfg=root/'private.json';cfg.write_text('{"private":"preserve"}')
            db=root/'history.sqlite'
            with sqlite3.connect(str(db)) as c:c.execute('CREATE TABLE samples(ts INTEGER)')
            before=db.read_bytes()
            self.assertIn('detenida',updater.update(root,db,True))
            with open(str(db)+'.lock','a') as lock:
                fcntl.flock(lock.fileno(),fcntl.LOCK_EX|fcntl.LOCK_NB)
                with self.assertRaises(BlockingIOError):updater.update(root,db)
            updater.update(root,db)
            self.assertIn('class Engine:',(pkg/'engine.py').read_text())
            self.assertEqual(cfg.read_text(),'{"private":"preserve"}')
            self.assertEqual(db.read_bytes(),before)
            self.assertEqual(len(list(root.glob('cielo_monitor_backup_*'))),1)
            self.assertEqual(len(list(root.glob('history_backup_*.sqlite'))),1)
    def test_unknown_v3_layout_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);old=root/'observador.py';old.write_text('# Sierra V3\n')
            with self.assertRaises(ValueError):updater.update(root,root/'history.sqlite')
            self.assertEqual(old.read_text(),'# Sierra V3\n')
if __name__=='__main__':unittest.main()
