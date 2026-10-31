import importlib.util,json,pathlib,tempfile,unittest,hashlib,plistlib
from unittest import mock
spec=importlib.util.spec_from_file_location('verify',pathlib.Path('tools/cielo_verify_sierra.py'))
v=importlib.util.module_from_spec(spec);spec.loader.exec_module(v)


class VerifierTests(unittest.TestCase):
    def test_private_only(self):
        for url in ['http://192.168.0.25','http://10.0.0.1:8080/','http://172.16.1.2']:
            self.assertTrue(v.local_url(url).startswith('http://'))
        for url in ['http://8.8.8.8','http://127.0.0.1','http://169.254.1.2','https://192.168.0.25','http://user:secret@192.168.0.25','http://example.com','http://192.168.0.25/api/save','http://192.168.0.25/?secret=x','http://192.168.0.25:bad']:
            with self.assertRaises(ValueError):v.local_url(url)

    def test_missing_and_redaction(self):
        for r in [{},{'alerts':None,'diagnostic_evidence':[],'curves':None,'bluetooth':False},
                  {'firmware':'PRIVATE_SECRET','ssid':'PRIVATE_SECRET','token':'PRIVATE_SECRET'}]:
            out=v.analyze(r)
            self.assertNotIn('PRIVATE_SECRET',json.dumps(out))
            checks={x['check']:x['result'] for x in out['findings']}
            self.assertEqual(checks['no_current_critical'],'UNKNOWN')
            self.assertEqual(checks['telegram_delivery'],'NOT_TESTED')

    def test_save_states(self):
        for state,expected in [('VERIFIED','PASS'),('UNCHANGED','PASS'),('FAILED','FAIL'),('UNCERTAIN','FAIL'),('NOT_ATTEMPTED_THIS_BOOT','NOT_TESTED')]:
            r={'diagnostic_evidence':{'schema_version':1,'curve_save':{'last_result':state}}}
            c={x['check']:x['result'] for x in v.analyze(r)['findings']}
            self.assertEqual(c['last_curve_save'],expected)

    def test_profile(self):
        p={'schema':'iwagumi-cielo-profile','schema_version':2,'curve_unit':'minute','lights':{
            'pezverde':{'points':[[839,0,0,0,0],[1440,0,0,0,0]]},
            'chihiros':{'points':[[870,1,1,1],[1440,0,0,0]]}}}
        wire='839,0,0,0,0;1440,0,0,0,0|870,1,1,1;1440,0,0,0';h=2166136261
        for b in wire.encode():h=((h^b)*16777619)&0xffffffff
        tag='{:08x}'.format(h);p['integrity']={'algorithm':'fnv1a32','checksum':tag}
        self.assertEqual(v.profile_checksum(p),tag)
        r={'curves':{'wire_checksum':tag}}
        self.assertIn({'check':'profile_identity','result':'PASS'},v.analyze(r,p)['findings'])
        p['integrity']['checksum']='00000000'
        self.assertIn({'check':'profile_identity','result':'INVALID_INPUT'},v.analyze(r,p)['findings'])

    def test_snapshot(self):
        s={'boot_id':142,'profileGeneration':14};r={'boot_id':142,'curves':{'generation':14}}
        self.assertEqual(v.same_snapshot(s,r,s),'PASS')
        self.assertEqual(v.same_snapshot(s,r,{'boot_id':143,'profileGeneration':14}),'RETRY_SNAPSHOT_CHANGED')
        self.assertEqual(v.same_snapshot({},r,s),'UNKNOWN')

    def test_no_execute_or_secrets(self):
        with tempfile.TemporaryDirectory() as t:
            app=pathlib.Path(t)/'Library'/'Application Support'/'IWAGUMI CIELO Vigilancia';app.mkdir(parents=True)
            raw=b'raise Exception("PRIVATE_SECRET")\n';(app/'cielo_observer.py').write_bytes(raw)
            (app/'config.json').write_text('PRIVATE_SECRET')
            result=v.inspect_observer(t,hashlib.sha256(raw).hexdigest())
            self.assertTrue(result['source_matches_repo_4_0_1'])
            self.assertFalse(result['database_opened'])
            self.assertFalse(result['running_version_verified'])
            self.assertNotIn('PRIVATE_SECRET',json.dumps(result))

    def test_get_only_no_proxy_redirect_size(self):
        response=mock.MagicMock();response.__enter__.return_value.read.return_value=b'{}'
        opener=mock.MagicMock();opener.open.return_value=response
        with mock.patch.object(v.urllib.request,'build_opener',return_value=opener) as build:
            self.assertEqual(v.get_json('http://192.168.0.25','/api/status'),{})
            self.assertEqual(opener.open.call_args[0][0].get_method(),'GET')
            self.assertEqual(build.call_args[0][0].proxies,{})
            response.__enter__.return_value.read.return_value=b'x'*(v.MAX_BYTES+1)
            with self.assertRaises(ValueError):v.get_json('http://192.168.0.25','/api/status')
        with self.assertRaises(ValueError):v.NoRedirect().redirect_request(None,None,302,'',{},'http://8.8.8.8')

    def test_output_permission(self):
        with tempfile.TemporaryDirectory() as t:
            source=pathlib.Path(t)/'private.json';source.write_text('{"token":"PRIVATE_SECRET"}')
            with mock.patch.object(v,'inspect_observer',return_value={'not_tested':True}):
                self.assertEqual(v.main(['--report',str(source),'--output-dir',str(pathlib.Path(t)/'out')]),0)
            out=next((pathlib.Path(t)/'out').glob('*.json'))
            self.assertEqual(out.stat().st_mode&0o777,0o600)
            self.assertNotIn('PRIVATE_SECRET',out.read_text())


if __name__=='__main__':unittest.main()
