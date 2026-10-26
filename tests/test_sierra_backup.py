"""No real USB/network. Check package and execute the actual read-only backup flow."""
from pathlib import Path
import hashlib,importlib.util,json,os,subprocess,sys,tempfile,types,unittest,zipfile
sys.path.insert(0,'tools')
from package_sierra_backup import package,FOLDER
import cielo_backup_sierra as backup
from cielo_flash import unseal
spec=importlib.util.spec_from_file_location('backup_flash_fixture',Path('tests/test_b89_flash.py'))
fixture=importlib.util.module_from_spec(spec);spec.loader.exec_module(fixture)

class FakeRunner:
    def __init__(self,raw,major=4,mismatch=False,fail_at=None):
        self.raw,self.major,self.mismatch,self.fail_at=raw,major,mismatch,fail_at
        self.commands=[];self.reads=0
    def __call__(self,cmd,**kwargs):
        self.commands.append(cmd)
        if cmd[-1]=='version':return types.SimpleNamespace(returncode=0,stdout='esptool.py v4.8.1\n4.8.1\n')
        command=next(c for c in cmd if c in ('read_mac','read_flash','read-mac','read-flash'))
        if self.fail_at==len(self.commands):return types.SimpleNamespace(returncode=1,stdout='private error')
        if command in ('read_mac','read-mac'):return types.SimpleNamespace(returncode=0,stdout='MAC: aa:bb:cc:dd:ee:ff\n')
        self.reads+=1
        Path(cmd[-1]).write_bytes(b'\0'*len(self.raw) if self.mismatch and self.reads==2 else self.raw)
        return types.SimpleNamespace(returncode=0,stdout='Read OK')

class SierraBackup(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.raw=fixture.image()
    def test_check_uses_only_offline_version(self):
        runner=FakeRunner(self.raw)
        r=backup.check_environment(runner,[{'device':'/dev/cu.usbmodem1','description':'ATOM','vid':123,'pid':456}])
        self.assertTrue(r['ready']);self.assertFalse(r['usb_opened']);self.assertFalse(r['network_used'])
        self.assertEqual(runner.commands,[[sys.executable,'-m','esptool','version']])
        r=backup.check_environment(runner,[]);self.assertFalse(r['ready'])
    def test_actual_read_sequence_and_versions(self):
        for major in (4,5):
            runner=FakeRunner(self.raw,major)
            tool=backup.ReadOnlyTool('/dev/cu.usbmodem1',major,runner)
            with tempfile.TemporaryDirectory() as d:raw,info,device=backup.capture(tool,d)
            self.assertEqual(raw,self.raw);self.assertEqual(info['size'],8388608)
            self.assertEqual(device,hashlib.sha256(b'aa:bb:cc:dd:ee:ff').hexdigest())
            token=lambda s:s.replace('_','-') if major==5 else s
            self.assertIn(token('default_reset'),runner.commands[0]);self.assertIn(token('no_reset'),runner.commands[0])
            self.assertIn(token('no_reset'),runner.commands[1]);self.assertNotIn(token('hard_reset'),runner.commands[1])
            self.assertIn(token('no_reset'),runner.commands[2]);self.assertIn(token('hard_reset'),runner.commands[2])
            self.assertEqual(len(runner.commands),3)
    def test_refuses_flash_mutation_and_wrong_ranges(self):
        runner=FakeRunner(self.raw);tool=backup.ReadOnlyTool('/dev/cu.usbmodem1',4,runner)
        for command in ('write_flash','erase_flash','erase_region','restore','write-flash'):
            self.assertRaises(ValueError,tool.read,command)
        self.assertRaises(ValueError,tool.read,'read_flash','0x0','0x1000','copy.bin')
        self.assertRaises(ValueError,tool.read,'read_mac',before='usb_reset')
        self.assertEqual(runner.commands,[])
    def test_mismatch_and_failure_do_not_certify(self):
        runner=FakeRunner(self.raw,mismatch=True)
        with tempfile.TemporaryDirectory() as d:self.assertRaises(ValueError,backup.capture,backup.ReadOnlyTool('/dev/cu.usbmodem1',4,runner),d)
        runner=FakeRunner(self.raw,fail_at=2)
        with tempfile.TemporaryDirectory() as d:self.assertRaises(RuntimeError,backup.capture,backup.ReadOnlyTool('/dev/cu.usbmodem1',4,runner),d)
        self.assertIn('hard_reset',runner.commands[-1]);self.assertIn('read_mac',runner.commands[-1])
    def test_encrypted_only_output_and_safe_report(self):
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/'private';path.mkdir(mode=0o700)
            raw=bytearray(self.raw);raw[0x9000+64:0x9000+80]=b'PRIVATE_PASSWORD';raw=bytes(raw)
            info=backup.inspect_flash(raw)
            report=backup.save_backup(raw,info,'device-hash','local-test-password-123',path)
            self.assertEqual(sorted(p.suffix for p in path.iterdir()),['.cieloenc','.json'])
            decoded,_,device=unseal((path/report['backup_file']).read_bytes(),'local-test-password-123')
            self.assertEqual(decoded,raw);self.assertEqual(device,'device-hash')
            self.assertNotIn('PRIVATE_PASSWORD',json.dumps(report));self.assertNotIn('local-test-password',json.dumps(report))
            for p in path.iterdir():self.assertEqual(p.stat().st_mode&0o777,0o600)
            self.assertRaises(ValueError,backup.save_backup,raw,info,'x','short',path)
    def test_packaged_helpers_and_shell_launchers(self):
        with tempfile.TemporaryDirectory() as d:
            dst=Path(d)/'backup.zip';p=package(Path('.'),dst)
            self.assertEqual(p['sha256'],hashlib.sha256(dst.read_bytes()).hexdigest())
            with zipfile.ZipFile(dst) as z:
                names={Path(n).name for n in z.namelist()}
                self.assertEqual(names,{'COMPROBAR.command','HACER_COPIA.command','LEEME.txt','cielo_python.sh','cielo_backup_sierra.py','cielo_backup_metadata.py','PAQUETE_SHA256.json'})
                for n in z.namelist():
                    if n.endswith(('.command','.sh')):
                        self.assertEqual((z.getinfo(n).external_attr>>16)&0o777,0o755)
                        subprocess.run(['bash','-n'],input=z.read(n),check=True)
                z.extractall(Path(d)/'unzip')
            code='import cielo_backup_metadata as m; assert m.MAGIC==b"CIELO8M1\\x00"; assert not hasattr(m,"Esptool"); assert not hasattr(m,"main")'
            subprocess.run([sys.executable,'-c',code],cwd=Path(d)/'unzip'/FOLDER,check=True)
            script=(Path(d)/'unzip'/FOLDER/'cielo_backup_sierra.py').read_text()
            self.assertNotIn('pip install',script)

if __name__=='__main__':unittest.main()
