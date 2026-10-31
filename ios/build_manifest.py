#!/usr/bin/env python3
"""Traceability of unsigned CI artifacts; never claims App Store availability."""
import hashlib,json,os,re,subprocess
from pathlib import Path

def run(*args):
    return subprocess.check_output(args,text=True).strip()

xcode=run('xcodebuild','-version')
sdk=run('xcrun','--sdk','iphoneos','--show-sdk-version')
major=int(re.search(r'Xcode (\d+)',xcode).group(1))
files={p.name:{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(Path('dist').glob('CIELO_*')) if p.is_file()}
ui_results={}
for name in ['CIELO_UI_RESUMEN.json','CIELO_UI_LARGE_RESUMEN.json']:
    path=Path('dist')/name
    ui_results[name]=json.loads(path.read_text()) if path.exists() else None
manifest={'app':'CIELO','version':'0.2.1','signed':False,'published':False,'ready_for_testflight':False,'ui_results':ui_results,'iphone_automated_checks_passed':all(r is not None and r.get('result')=='Passed' and r.get('failedTests')==0 for r in ui_results.values()),'installable_on_user_iphone':False,'ci_commit':run('git','rev-parse','HEAD'),'source_commit':os.environ.get('CIELO_SOURCE_SHA',''),'run':os.environ.get('GITHUB_RUN_ID'),'xcode':xcode,'iphoneos_sdk':sdk,'meets_sdk_minimum_as_of_2026_10_31':major>=26 and int(sdk.split('.')[0])>=26,'tests':'CoreChecks + ControllerChecks + iPhone XCUITest including selected accessibility audits and landscape; separate iPad artifact required; real hardware pending','files':files}
Path('dist/BUILD_MANIFEST.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
