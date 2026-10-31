#!/usr/bin/env python3
"""Offline delivery verification. Passing CI never implies signing or App Review."""
import hashlib,json,os,sys,zipfile
from pathlib import Path
root=Path(__file__).resolve().parent.parent
bundle=Path(sys.argv[1]);expected=os.environ['CIELO_SOURCE_SHA']
def read(name):return json.loads((bundle/name).read_text())
phone=read('BUILD_MANIFEST.json');ipad=read('CIELO_IPAD_MANIFIESTO.json')
for manifest in [phone,ipad]:
    assert manifest['source_commit']==expected,'Source commit mismatch'
    assert manifest['version']=='0.2.1','Version mismatch'
    for name,info in manifest['files'].items():
        data=(bundle/name).read_bytes()
        assert len(data)==info['bytes'] and hashlib.sha256(data).hexdigest()==info['sha256'],name
assert phone['run']==ipad['run'] and phone['ci_commit']==ipad['ci_commit'],'Different CI provenance'
assert phone['signed'] is False and phone['published'] is False
assert phone['meets_sdk_minimum_as_of_2026_10_31'] is True
results={}
for family,name in [('iphone','CIELO_UI_RESUMEN.json'),('ipad','CIELO_IPAD_UI_RESUMEN.json')]:
    summary=read(name)
    assert summary['result']=='Passed' and summary['failedTests']==0 and summary['skippedTests']==0,name
    assert summary['passedTests']==6 and summary['totalTestCount']==6,name
    results[family]={'passed':summary['passedTests'],'failed':0,'skipped':0,'devices':summary['devicesAndConfigurations']}
archive=zipfile.ZipFile(bundle/'CIELO_IOS_FUENTES.zip');count=0
for file in sorted((root/'ios').rglob('*')):
    if file.is_file() and '__pycache__' not in file.parts:
        name=file.relative_to(root).as_posix()
        assert archive.read(name)==file.read_bytes(),name
        count+=1
# The final JSON states the remaining human/account/device requirements explicitly.
report={'version':'0.2.1','source_commit':expected,'ci_commit':phone['ci_commit'],'run':phone['run'],'automated_checks_passed':True,'sources_verified':count,'tests':results,'signed':False,'published':False,'ready_for_testflight':False,'pending':['APPLE_DEVELOPER_SIGNING','REAL_IPHONE_ATOM_TEST','OWNER_STORE_METADATA_AND_URLS','MANUAL_ACCESSIBILITY_REVIEW']}
(bundle/'VERIFICACION_ENTREGA.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print(f'PASS delivery: {count} source files, artifact hashes and 12 UI tests; signing/hardware/review remain pending')
