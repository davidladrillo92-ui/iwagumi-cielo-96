#!/usr/bin/env python3
"""Sierra Python3.9: check tools offline, then two USB READS and encrypted backup.
No install, write_flash, erase, restore, OTA, network or observer modification.
"""
import argparse
import datetime
import getpass
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import sys
import tempfile

try:
    from cielo_backup_metadata import inspect_flash,seal,unseal,private_write
except ImportError:
    from cielo_flash import inspect_flash,seal,unseal,private_write

def available_ports():
    try:
        from serial.tools import list_ports
        result=[]
        for p in list_ports.comports():
            if sys.platform=='darwin' and not p.device.startswith('/dev/cu.'):
                continue
            if p.vid is not None or any(t in p.device.lower() for t in ('usb','acm')):
                result.append({'device':p.device,'description':p.description or 'USB',
                               'vid':p.vid,'pid':p.pid})
        return sorted(result,key=lambda p:p['device'])
    except ImportError:
        return []

def check_environment(runner=subprocess.run,ports=None):
    report={'schema':'cielo-sierra-backup-check-v1','python':platform.python_version(),
            'macos':platform.mac_ver()[0] or None,'usb_opened':False,'network_used':False,
            'esptool_version':None,'encryption_available':False,'issues':[]}
    if sys.version_info<(3,9):report['issues'].append('Se requiere Python3.9 o posterior; en Sierra usa3.9.')
    try:
        # version is an offline esptool command, with no port or chip connection.
        r=runner([sys.executable,'-m','esptool','version'],capture_output=True,text=True,timeout=20)
        match=re.search(r'\b([45]\.\d+(?:\.\d+)?)\b',r.stdout or '')
        if r.returncode or not match:raise RuntimeError('esptool no disponible')
        report['esptool_version']=match.group(1)
    except (OSError,subprocess.SubprocessError,RuntimeError):
        report['issues'].append('No se puede ejecutar esptool4/5 con este Python; no se instala nada automaticamente.')
    try:
        from cryptography.hazmat.primitives.ciphers.aead import AESGCM
        from cryptography.hazmat.primitives.kdf.scrypt import Scrypt
        key=Scrypt(salt=b'cielo-local-check',length=32,n=2,r=1,p=1).derive(b'check-only')
        aes=AESGCM(key);nonce=b'\0'*12;data=aes.encrypt(nonce,b'check-only',None)
        if aes.decrypt(nonce,data,None)!=b'check-only':raise RuntimeError('Cifrado no operativo')
        report['encryption_available']=True
    except Exception:
        report['issues'].append('El cifrado de respaldo no esta disponible en este Python.')
    report['ports']=available_ports() if ports is None else ports
    report['ready']=not report['issues'] and bool(report['ports'])
    if not report['ports']:report['issues'].append('No se observa puerto USB del ATOM: conecta un cable de datos y comprueba de nuevo.')
    return report

class ReadOnlyTool:
    def __init__(self,port,major,runner=subprocess.run):
        if not port or port.startswith('-') or major not in (4,5):raise ValueError('Puerto/version no validos')
        self.port,self.major,self.runner=port,major,runner
    def read(self,command,*args,before='no_reset',after='no_reset'):
        if command not in ('read_mac','read_flash'):raise ValueError('Solo lecturas USB permitidas')
        if before not in ('default_reset','no_reset') or after not in ('no_reset','hard_reset'):
            raise ValueError('Secuencia de reinicio no valida')
        if command=='read_mac' and args:raise ValueError('Argumentos inesperados')
        if command=='read_flash' and (len(args)!=3 or args[:2]!=('0x0','0x800000')):
            raise ValueError('Solo copia completa de8MiB permitida')
        if self.major==5:command,before,after=[v.replace('_','-') for v in (command,before,after)]
        cmd=[sys.executable,'-m','esptool','--chip','esp32s3','--port',self.port,'--baud','460800',
             '--before',before,'--after',after,command,*map(str,args)]
        r=self.runner(cmd,capture_output=True,text=True,timeout=900 if args else 30)
        if r.returncode:raise RuntimeError('La lectura USB fallo. No se fuerza acceso ni se borra flash.')
        return r.stdout

def capture(tool,directory):
    directory=Path(directory);first=directory/'read1.bin';second=directory/'read2.bin'
    completed=False
    try:
        out=tool.read('read_mac',before='default_reset',after='no_reset')
        macs=re.findall(r'(?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}',out)
        if not macs:raise RuntimeError('No se pudo identificar el ATOM; copia detenida')
        device=hashlib.sha256(macs[-1].lower().encode()).hexdigest()
        # The application cannot run between these reads and change NVS.
        tool.read('read_flash','0x0','0x800000',first,before='no_reset',after='no_reset')
        tool.read('read_flash','0x0','0x800000',second,before='no_reset',after='hard_reset')
        completed=True
        a=first.read_bytes();b=second.read_bytes()
        if a!=b:raise ValueError('Las dos lecturas difieren: no se certifica ni guarda el respaldo')
        info=inspect_flash(a)
        return a,info,device
    finally:
        if not completed:
            try:tool.read('read_mac',before='no_reset',after='hard_reset')
            except Exception:pass # The error message explains manual power-cycle recovery.

def save_backup(raw,info,device,password,directory):
    if len(password)<12:raise ValueError('Usa una contrasena de al menos12 caracteres')
    directory=Path(directory);directory.mkdir(parents=True,exist_ok=True,mode=0o700)
    if directory.stat().st_mode&0o077:raise ValueError('La carpeta de respaldo debe ser privada (permisos0700)')
    encrypted=seal(raw,password,device)
    check,checked_info,checked_device=unseal(encrypted,password)
    if check!=raw or checked_info['sha256']!=info['sha256'] or checked_device!=device:
        raise ValueError('El respaldo cifrado no supera la verificacion')
    token=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%d_%H%M%S')+'_'+os.urandom(3).hex()
    filename='CIELO_COPIA_ACTUAL_'+token+'.cieloenc'
    report={'schema':'cielo-current-backup-v1','two_reads_match':True,'encrypted_verified':True,
            'backup_file':filename,'device_id_sha256':device,'flash':info,
            'raw_files_retained':False,'format_attempted':False,'flash_write_attempted':False,
            'note':'Informe de metadatos; no incluye credenciales ni contenido de NVS.'}
    private_write(directory/filename,encrypted)
    private_write(directory/('CIELO_INFORME_COPIA_'+token+'.json'),json.dumps(report,indent=2).encode())
    return report

def main():
    os.umask(0o077)
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--backup',action='store_true');p.add_argument('--directory',type=Path,required=True)
    a=p.parse_args();a.directory.mkdir(parents=True,exist_ok=True,mode=0o700)
    if a.backup and a.directory.stat().st_mode&0o077:raise ValueError('La carpeta de respaldo debe ser privada (0700)')
    report=check_environment()
    check_name=a.directory/('COMPROBACION_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S')+'_'+os.urandom(2).hex()+'.json')
    private_write(check_name,json.dumps(report,indent=2).encode())
    print('Python:',report['python'],'· esptool:',report['esptool_version'] or 'no disponible')
    for i,port in enumerate(report['ports'],1):print(str(i)+'. '+port['device']+' · '+port['description'])
    print('Comprobacion guardada:',check_name)
    if report['issues']:
        for issue in report['issues']:print(issue)
        return 1
    if not a.backup:
        print('Comprobacion terminada. No se abrio USB ni se reinicio el ATOM.');return 0
    if not sys.stdin.isatty():raise ValueError('Abre HACER_COPIA.command en Terminal para introducir la contrasena de forma privada')
    print('La copia pausara/reiniciara el ATOM. Prepara las luces para funcionar sin su control durante la lectura.')
    print('Se leera dos veces; puede tardar varios minutos. El vigilante puede avisar durante esta pausa.')
    if input('Escribe COPIAR para continuar, o pulsa Intro para salir: ').strip()!='COPIAR':return 0
    if len(report['ports'])==1:port=report['ports'][0]['device']
    else:
        choice=input('Numero del puerto USB del ATOM: ').strip()
        if not choice.isdigit() or not 1<=int(choice)<=len(report['ports']):raise ValueError('Seleccion de puerto no valida')
        port=report['ports'][int(choice)-1]['device']
    password=getpass.getpass('Contrasena nueva para cifrar la copia (minimo12 caracteres): ')
    if password!=getpass.getpass('Repite la contrasena: '):raise ValueError('Las contrasenas no coinciden')
    if len(password)<12:raise ValueError('Usa al menos12 caracteres')
    major=int(report['esptool_version'].split('.')[0]);tool=ReadOnlyTool(port,major)
    # Explicitly local: raw temporary copies remain private and are removed.
    with tempfile.TemporaryDirectory(prefix='lectura_privada_',dir=a.directory) as temp:
        raw,info,device=capture(tool,temp)
        result=save_backup(raw,info,device,password,a.directory)
    print('Copia cifrada y verificada:',result['backup_file'])
    print('Conserva la copia y su contrasena en privado. Puedes compartir el JSON de metadatos.')
    return 0

if __name__=='__main__':
    try:raise SystemExit(main())
    except (Exception,KeyboardInterrupt) as e:
        print('Copia detenida:',str(e),file=sys.stderr)
        print('Si el ATOM no retoma el programa, desconecta y vuelve a conectar su alimentacion. No borres ni flashees nada.',file=sys.stderr)
        raise SystemExit(1)
