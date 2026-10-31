#!/usr/bin/env python3
"""Create a separate verification bundle; never include configuration or DB files."""
from pathlib import Path
import zipfile,hashlib,json
folder='CIELO_COMPROBACION_SIERRA_V1'
command='''#!/bin/bash
cd "$(dirname "$0")" || exit 1
. ./cielo_python.sh || { read -r -p "Pulsa INTRO para cerrar..." z; exit 1; }
read -r -p "IP del ATOM [192.168.0.25]: " atom_ip
atom_ip=${atom_ip:-192.168.0.25}
"$CIELO_PYTHON" cielo_verify_sierra.py --url "http://$atom_ip"
res=$?
echo
read -r -p "Pulsa INTRO para cerrar..." z
exit "$res"
'''
readme='''CIELO — COMPROBACION DE SOLO LECTURA PARA iMAC SIERRA

1. Descomprime el ZIP y abre COMPROBAR_CIELO.command.
2. Pulsa Intro si el ATOM sigue en192.168.0.25.
3. Envia el JSON creado dentro de RESULTADOS.

Requiere el Python3.9 que ya usa el vigilante. No instala dependencias.
Lee tres respuestas HTTP locales: status, Informe Maestro compartible, status.
No guarda las respuestas completas: genera un resumen de campos permitidos.
No cambia luces, perfiles, Wi-Fi, credenciales ni servicios y no envia Telegram.
No lee configuraciones, registros de texto ni bases de datos del vigilante.
Compara el archivo instalado con la fuente conocida4.0.1 y consulta su registro
en launchd; eso no certifica que este tomando muestras ni que Telegram llegue.

Si macOS bloquea la apertura, usa boton derecho > Abrir. No es necesario sudo.

Uso opcional desde Terminal:
python3.9 cielo_verify_sierra.py --report INFORME.json --profile PERFIL.json
La comparacion exacta del perfil requiere un exportado y un informe3.12 con checksum.
Sin ese perfil, su identidad se marca NO PROBADA, aunque el contenido sea valido.
La herramienta detecta fallos de lectura y no considera la ausencia de datos como exito.
La validacion optica, Telegram real, reinicios y perdidas de enlace requieren pruebas aparte.
'''
source=Path('monitor/macos_sierra/cielo_observer.py').read_bytes()
identity=Path('tools/IDENTIDAD.json').read_bytes()
assert json.loads(identity)['observer_sha256']==hashlib.sha256(source).hexdigest()
files={'cielo_verify_sierra.py':Path('tools/cielo_verify_sierra.py').read_bytes(),
       'IDENTIDAD.json':identity,'cielo_python.sh':Path('monitor/macos_sierra/cielo_python.sh').read_bytes(),
       'COMPROBAR_CIELO.command':command.encode(),'LEEME.txt':readme.encode()}
out=Path('dist')/(folder+'.zip');out.parent.mkdir(exist_ok=True)
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as z:
    for name,content in files.items():
        info=zipfile.ZipInfo(folder+'/'+name);info.create_system=3
        info.external_attr=(0o100755 if name.endswith(('.command','.sh')) else 0o100644)<<16
        info.compress_type=zipfile.ZIP_DEFLATED;z.writestr(info,content)
print(json.dumps({'file':str(out),'bytes':out.stat().st_size,'sha256':hashlib.sha256(out.read_bytes()).hexdigest()}))
