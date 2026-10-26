#!/usr/bin/env python3
"""Suite completa heredada + B8/B9. No USB, MQTT, Telegram ni ATOM reales."""
from pathlib import Path
import subprocess
import sys
import tempfile

def run(*args):subprocess.run(args,check=True)
run(sys.executable,'tools/validar_fuentes.py')
run(sys.executable,'tools/test_security_crypto.py')
with tempfile.TemporaryDirectory() as t:
 for source in sorted(Path('tests').glob('test_*.cpp')):
  output=str(Path(t)/source.stem)
  extra=['src/cielo_monitor.cpp'] if source.stem=='test_b89_monitor' else []
  mock=['-Itests/mockfs'] if source.stem in ('test_rc5_3_5_fs_store','test_fs_active_guard','test_fs_v5_profile','test_storage_mount_probe','test_littlefs_readonly_probe') else []
  run('g++','-std=c++17','-Wall','-Wextra','-Werror',*mock,'-Iinclude',str(source),*extra,'-o',output)
  run(output)
 for source in sorted(Path('tests').glob('test_*.js')):run('node',str(source))
for source in sorted(Path('tests').glob('test_*.py')):
 run(sys.executable,str(source))
print('PASS: todas las suites host; no sustituye hardware físico')
