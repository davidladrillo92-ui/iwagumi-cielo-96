#!/usr/bin/env python3
"""Package the Sierra installer without configuration, DB or generated caches."""
from pathlib import Path
import hashlib
import json
import zipfile

source=Path('monitor/macos_sierra')
expected={'ACTUALIZAR.command','INSTALAR.command','CONSULTAR_ESTADO.command',
    'DETENER_VIGILANCIA.command','PROBAR_TELEGRAM.command','VOLVER_A_VERSION_ANTERIOR.command',
    'cielo_python.sh','cielo_observer.py','telegram_ca.pem','LICENSE_CERTIFICADOS.txt','LEEME.txt'}
assert expected=={p.name for p in source.iterdir() if p.is_file()}
assert hashlib.sha256((source/'telegram_ca.pem').read_bytes()).hexdigest()=='7cef540c6af3bf0a3c7d0070bd3fd7aae34f8aa805b408d1c27a01c8aeb305df'
folder='CIELO_VIGILANCIA_iMAC_SIERRA_V4'
Path('dist').mkdir(exist_ok=True)
target=Path('dist')/(folder+'.zip')
with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED) as z:
    for name in sorted(expected):
        info=zipfile.ZipInfo(folder+'/'+name)
        info.create_system=3
        info.external_attr=((0o100755 if name.endswith(('.command','.sh')) else 0o100644)<<16)
        info.compress_type=zipfile.ZIP_DEFLATED
        z.writestr(info,(source/name).read_bytes())
digest=hashlib.sha256(target.read_bytes()).hexdigest()
print(json.dumps({'file':str(target),'bytes':target.stat().st_size,'sha256':digest,'version':'4.0.0-SIERRA'}))
