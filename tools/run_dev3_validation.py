#!/usr/bin/env python3
"""Reproduce the DEV3 host regression battery from the repository root.
Usage: python3 tools/run_dev3_validation.py /tmp/validacion.json
Requires Python, g++, Node and the dependencies of the inherited tests.
Does not compile firmware or exercise physical hardware.
"""
import json,subprocess,concurrent.futures,tempfile,pathlib,sys,os
os.chdir(pathlib.Path(__file__).resolve().parents[1])
base=json.load(open('docs/VALIDACION_DEV2_FINAL.json')); tests=[x['test'] for x in base['tests']]+['test_history315.cpp','test_report315.cpp','test_report315_ui.js','test_dev3_contract.py']
mock={'test_rc5_3_5_fs_store','test_fs_active_guard','test_fs_v5_profile','test_storage_mount_probe','test_littlefs_readonly_probe','test_littlefs_posix'}
def run(name):
 with tempfile.TemporaryDirectory() as d:
  if name.endswith('.cpp'):
   stem=pathlib.Path(name).stem; exe=d+'/test';cmds=[['g++','-std=c++17','-Wall','-Wextra','-Werror','-Iinclude']+(['-Itests/mockfs'] if stem in mock else ['-Itests/mockwifi'] if stem=='test_wifi_nvs' else [])+['tests/'+name,'-o',exe],[exe]]
  elif name.endswith('.py'):cmds=[[sys.executable,'tests/'+name]]
  elif name.endswith('.js'):cmds=[['node','tests/'+name]]
  else:cmds=[[sys.executable,{'validar_fuentes':'tools/validar_fuentes.py','security_crypto':'tools/test_security_crypto.py','littlefs_lab':'tools/check_littlefs_migration_lab.py'}[name]]]
  out='';code=0
  for c in cmds:
   try:r=subprocess.run(c,capture_output=True,text=True,timeout=120);out+=r.stdout+r.stderr;code=r.returncode
   except Exception as e:out+=str(e);code=1
   if code:break
  return {'test':name,'code':code,'output':out[-6000:]}
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as ex: results=list(ex.map(run,tests))
print('PASS',sum(x['code']==0 for x in results),'/',len(results),flush=True)
for r in results:
 if r['code']:print(r['test'],r['output'],flush=True)
with open(sys.argv[1] if len(sys.argv)>1 else '/tmp/validacion-dev3.json','w') as out:
 json.dump({'scope':'HOST_ONLY','passed':sum(x['code']==0 for x in results),'failed':sum(x['code']!=0 for x in results),'tests':results},out,indent=2)
sys.exit(int(any(x['code'] for x in results)))
