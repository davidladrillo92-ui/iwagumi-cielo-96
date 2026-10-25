#!/usr/bin/env python3
"""Update an identified package installation while preserving configuration and SQLite.
Requires the observer to be stopped. Does not send Telegram or contact the ATOM.
"""
import argparse
import ast
import datetime as dt
import fcntl
import os
from pathlib import Path
import shutil
import sys


def update(root, database, check=False):
    os.umask(0o077)
    root, database = Path(root).expanduser().resolve(), Path(database).expanduser().resolve()
    target = root/'cielo_monitor'
    source = Path(__file__).resolve().parents[1]/'monitor/cielo_monitor'
    if not (target/'engine.py').is_file() or not (target/'__main__.py').is_file():
        raise ValueError('La instalación no es el paquete cielo_monitor. El instalador Sierra V3 puede ser distinto; no se ha modificado nada.')
    if not database.is_file():
        raise ValueError('No se identifica el histórico actual. Indica la base SQLite real; no se ha modificado nada.')
    if source.resolve() == target.resolve():
        raise ValueError('Elige la instalación del iMac, no la carpeta de este paquete.')
    for path in source.glob('*.py'):
        ast.parse(path.read_text(), feature_version=(3, 9))
    with open(str(database)+'.lock','a') as lock:
        fcntl.flock(lock.fileno(),fcntl.LOCK_EX|fcntl.LOCK_NB)
        if check:
            return 'Instalación identificada y detenida. Configuración, claves e histórico se conservarán.'
        stamp=dt.datetime.now().strftime('%Y%m%d_%H%M%S_%f')
        backup=root/('cielo_monitor_backup_'+stamp)
        staging=root/('cielo_monitor_new_'+stamp)
        shutil.copytree(source,staging,ignore=shutil.ignore_patterns('__pycache__','*.pyc'))
        # Copy a consistent SQLite snapshot; leave the live DB and all credentials untouched.
        import sqlite3
        with sqlite3.connect(str(database)) as origin, sqlite3.connect(str(root/('history_backup_'+stamp+'.sqlite'))) as destination:
            origin.backup(destination)
        target.rename(backup)
        try:
            staging.rename(target)
        except BaseException:
            backup.rename(target)
            raise
        return 'Observador 4.0.0 preparado. Reinicia su LaunchAgent habitual; conserva la carpeta backup para volver atrás.'


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('installation',help='Carpeta que contiene cielo_monitor')
    parser.add_argument('--database',required=True,help='Ruta de la base SQLite actual')
    parser.add_argument('--check',action='store_true')
    args=parser.parse_args()
    if sys.version_info < (3,9):
        raise SystemExit('Se necesita Python 3.9 o posterior.')
    try:
        print(update(args.installation,args.database,args.check))
    except (OSError,ValueError) as exc:
        print('No se actualiza: '+(str(exc) if isinstance(exc,ValueError) else 'detén primero el observador y comprueba las rutas/permisos.'))
        raise SystemExit(1)

if __name__=='__main__':main()
