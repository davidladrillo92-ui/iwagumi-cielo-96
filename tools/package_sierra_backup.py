#!/usr/bin/env python3
"""Package a read-only USB backup helper, no observer/config/private files."""
from pathlib import Path
import ast
import hashlib
import json
import zipfile

FOLDER='CIELO_COPIA_USB_SIERRA_V1_0_0'

def package(root=Path('.'),output=None):
    root=Path(root);output=Path(output) if output else root/'dist'/(FOLDER+'.zip')
    source=(root/'tools/cielo_flash.py').read_text()
    tree=ast.parse(source)
    allowed=('nvs_page_inventory','inspect_flash','derive','seal','unseal','private_write')
    functions={node.name:ast.get_source_segment(source,node) for node in tree.body if isinstance(node,ast.FunctionDef) and node.name in allowed}
    if set(functions)!=set(allowed):raise ValueError('Metadata/crypto helpers changed; review package')
    metadata='import base64,hashlib,json,os,re,struct\nfrom pathlib import Path\nFLASH_SIZE=0x800000\nPARTITIONS_OFFSET=0x8000\nMAGIC=b"CIELO8M1\\x00"\n\n'
    metadata+='\n\n'.join(functions[name] for name in allowed)+'\n'
    files={p.name:p.read_bytes() for p in (root/'backup/macos_sierra').iterdir() if p.is_file()}
    if set(files)!={'COMPROBAR.command','HACER_COPIA.command','LEEME.txt'}:raise ValueError('Unexpected backup package files')
    files['cielo_python.sh']=(root/'monitor/macos_sierra/cielo_python.sh').read_bytes()
    files['cielo_backup_sierra.py']=(root/'tools/cielo_backup_sierra.py').read_bytes()
    files['cielo_backup_metadata.py']=metadata.encode()
    for name,data in files.items():
        if name.endswith('.py'):ast.parse(data.decode(),feature_version=(3,9))
    manifest={'schema':'cielo-sierra-backup-package-v1','version':'1.0.0','python':'3.9',
              'flash_operations':['read_mac','read_flash'],'automatic_dependency_install':False,
              'files':{name:hashlib.sha256(data).hexdigest() for name,data in sorted(files.items())}}
    files['PAQUETE_SHA256.json']=(json.dumps(manifest,indent=2)+'\n').encode()
    output.parent.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED) as z:
        for name,data in sorted(files.items()):
            info=zipfile.ZipInfo(FOLDER+'/'+name);info.create_system=3
            info.external_attr=((0o100755 if name.endswith(('.sh','.command')) else 0o100644)<<16)
            info.compress_type=zipfile.ZIP_DEFLATED;z.writestr(info,data)
    return {'file':str(output),'bytes':output.stat().st_size,'sha256':hashlib.sha256(output.read_bytes()).hexdigest()}

if __name__=='__main__':print(json.dumps(package()))
