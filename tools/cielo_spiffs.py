#!/usr/bin/env python3
"""Lista archivos en una imagen SPIFFS privada ya extraída. No contacta hardware."""
import argparse
from pathlib import Path
import subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('image');p.add_argument('--mkspiffs',required=True,help='Ruta al ejecutable mkspiffs compatible')
p.add_argument('--block',type=int,required=True);p.add_argument('--page',type=int,required=True)
a=p.parse_args()
if a.block<=0 or a.page<=0 or a.block%a.page or not Path(a.image).is_file():
 p.error('Verifica imagen y geometría real del filesystem; no adivinar formato')
subprocess.run([a.mkspiffs,'-l','-b',str(a.block),'-p',str(a.page),a.image],check=True,timeout=60)
