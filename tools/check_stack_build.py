#!/usr/bin/env python3
"""Verify the linked Arduino loop stack override and compiler frame reports.
Static frame sizes do not prove the dynamic worst-case TLS stack requirement.
"""
import hashlib,json,re,subprocess
from pathlib import Path
root=Path('.pio/build/atoms3_lite_c124');elf=root/'firmware.elf'
nms=list((Path.home()/'.platformio/packages').rglob('xtensa-esp32s3-elf-nm'))
if not nms:raise SystemExit('No se localiza nm de ESP32-S3')
symbols=subprocess.check_output([str(nms[0]),'-C',str(elf)],text=True)
match=re.search(r'^([0-9a-fA-F]+)\s+T\s+getArduinoLoopTaskStackSize(?:\(\))?$',symbols,re.M)
if not match:raise SystemExit('La reserva loopTask no tiene override fuerte enlazado')
source=Path('src/main.cpp').read_text()
if 'CIELO_LOOP_STACK_BYTES=16384' not in source or 'SET_LOOP_TASK_STACK_SIZE(CIELO_LOOP_STACK_BYTES)' not in source:
 raise SystemExit('La reserva loopTask no coincide con 16384 B')
frames={}
for unit,limit in [('main.cpp',8192),('cielo_https_proxy.cpp',4096)]:
 files=list((root/'src').glob(unit+'.su'))
 if len(files)!=1:raise SystemExit('Falta informe de pila del compilador: '+unit)
 records=[]
 for line in files[0].read_text().splitlines():
  parts=line.rsplit('\t',2)
  if len(parts)==3 and parts[1].isdigit():
   records.append({'function':parts[0],'frame_bytes':int(parts[1]),'allocation':parts[2]})
 if not records:raise SystemExit('Informe de pila vacío: '+unit)
 records.sort(key=lambda v:v['frame_bytes'],reverse=True)
 if records[0]['frame_bytes']>=limit:raise SystemExit('Marco local excesivo: '+unit)
 frames[unit]=records[:8]
report={'schema':'cielo-stack-build-v1','loop_task_bytes':16384,
 'linked_override':{'symbol':'getArduinoLoopTaskStackSize','binding':'T','address':match[1]},
 'elf_sha256':hashlib.sha256(elf.read_bytes()).hexdigest(),'largest_compiler_frames':frames,
 'limitation':'Static frames only; physical LAN/OTA/BLE high-water marks and stability remain pending'}
Path('dist/STACK_REPORT.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
