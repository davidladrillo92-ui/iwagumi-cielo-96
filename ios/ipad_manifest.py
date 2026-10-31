#!/usr/bin/env python3
import hashlib,json,os,subprocess
from pathlib import Path
files={p.name:{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(Path('dist').glob('CIELO_IPAD_*')) if p.is_file()}
manifest={'version':'0.2.1','source_commit':os.environ['CIELO_SOURCE_SHA'],'ci_commit':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'run':os.environ['GITHUB_RUN_ID'],'files':files,'hardware_tested':False}
Path('dist/CIELO_IPAD_MANIFIESTO.json').write_text(json.dumps(manifest,indent=2)+'\n')
