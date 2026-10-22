#!/usr/bin/env python3
"""B9: respaldo USB ESP32-S3 de 8 MiB, cifrado, inspección y restauración explícita.
No requiere ATOM para inspect/pack. Nunca publica NVS ni claves.
"""
import argparse
import base64
import getpass
import hashlib
import importlib.metadata
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile

FLASH_SIZE = 0x800000
MAGIC = b'CIELO8M1\x00'
PARTITIONS_OFFSET = 0x8000


def nvs_page_inventory(raw):
    """Solo cabeceras/bitmap NVS IDF4; no claves, valores ni descifrado."""
    states = {0xfffffffe: 'active', 0xfffffffc: 'full', 0xfffffff8: 'freeing'}
    pages = []
    for offset in range(0, len(raw), 4096):
        page = raw[offset:offset+4096]
        if len(page) != 4096:
            raise ValueError('NVS no alineada')
        if page == b'\xff'*4096:
            pages.append({'page': offset//4096, 'state': 'empty', 'slots_empty': 126})
            continue
        state = states.get(int.from_bytes(page[:4], 'little'), 'unknown')
        entry = {'page': offset//4096, 'state': state}
        if state != 'unknown':
            counts = {'empty': 0, 'written': 0, 'erased': 0, 'invalid': 0}
            labels = {3: 'empty', 2: 'written', 0: 'erased', 1: 'invalid'}
            for n in range(126):
                bits = (page[32+n//4] >> (2*(n%4))) & 3
                counts[labels[bits]] += 1
            entry.update({'slots_'+k: v for k, v in counts.items()})
        pages.append(entry)
    return {'method': 'NVS_PAGE_BITMAP_ONLY_NOT_KEY_OR_CRC_VALIDATION', 'pages': pages,
            'note': 'Slots erased no son basura que borrar; NVS los recupera. No equivale a espacio util de un blob.'}


def inspect_flash(raw):
    if len(raw) != FLASH_SIZE:
        raise ValueError('El respaldo debe tener exactamente 8388608 bytes')
    table = raw[PARTITIONS_OFFSET:PARTITIONS_OFFSET+0x1000]
    entries, seen, end, md5_checked = [], set(), 0, False
    for i in range(0, len(table), 32):
        chunk = table[i:i+32]
        magic = struct.unpack_from('<H', chunk)[0]
        if magic == 0xffff:
            break
        if magic == 0xebeb:
            if hashlib.md5(table[:i]).digest() != chunk[16:32]:
                raise ValueError('Checksum MD5 de la tabla de particiones incorrecto')
            md5_checked = True
            break
        if magic != 0x50aa:
            raise ValueError('Tabla de particiones inválida a 0x8000')
        _, typ, subtype, offset, size, label, flags = struct.unpack('<HBBII16sI', chunk)
        name = label.rstrip(b'\x00').decode('ascii')
        if not re.fullmatch('[A-Za-z0-9_-]{1,16}', name) or name in seen:
            raise ValueError('Nombre de partición inválido o repetido')
        if offset < 0x9000 or size <= 0 or offset+size > FLASH_SIZE or offset < end:
            raise ValueError('Particiones solapadas o fuera de 8 MiB')
        if offset % 0x1000 or size % 0x1000:
            raise ValueError('Partición no alineada a sector')
        if typ == 0 and offset % 0x10000:
            raise ValueError('Aplicación no alineada')
        entry = dict(label=name, type=typ, subtype=subtype, offset=offset, size=size, flags=flags)
        body = raw[offset:offset+size]
        entry['all_bytes_ff'] = body == b'\xff'*size
        if typ == 1 and subtype == 2:
            entry['nvs_inventory'] = ({'status': 'ENCRYPTED_NOT_INSPECTED'} if flags & 1
                                      else nvs_page_inventory(body))
        entries.append(entry)
        end = offset+size
        seen.add(name)
    required = {'nvs', 'otadata', 'app0', 'app1'}
    if not required.issubset(seen):
        raise ValueError('Faltan NVS, otadata o las dos aplicaciones')
    return {'size': len(raw), 'sha256': hashlib.sha256(raw).hexdigest(),
            'partition_table_md5_verified': md5_checked, 'partitions': entries,
            'contents': 'NVS privada; filesystem no montado ni formateado'}


def derive(password, salt):
    from cryptography.hazmat.primitives.kdf.scrypt import Scrypt
    return Scrypt(salt=salt, length=32, n=32768, r=8, p=1).derive(password.encode())


def seal(raw, password, device=None):
    from cryptography.hazmat.primitives.ciphers.aead import AESGCM
    info = inspect_flash(raw)
    if len(password) < 12:
        raise ValueError('Contraseña de respaldo: mínimo 12 caracteres')
    salt, nonce = os.urandom(16), os.urandom(12)
    payload = json.dumps({'schema': 1, 'device_sha256': device, 'info': info,
                          'flash': base64.b64encode(raw).decode()}).encode()
    return MAGIC+salt+nonce+AESGCM(derive(password, salt)).encrypt(nonce, payload, MAGIC)


def unseal(blob, password):
    from cryptography.hazmat.primitives.ciphers.aead import AESGCM
    if not blob.startswith(MAGIC) or not 100 < len(blob) < FLASH_SIZE*2:
        raise ValueError('Archivo de respaldo desconocido o tamaño incorrecto')
    a = len(MAGIC)
    try:
        payload = AESGCM(derive(password, blob[a:a+16])).decrypt(blob[a+16:a+28], blob[a+28:], MAGIC)
        data = json.loads(payload)
        raw = base64.b64decode(data['flash'], validate=True)
        info = inspect_flash(raw)
        if data['schema'] != 1 or info['sha256'] != data['info']['sha256']:
            raise ValueError('Integridad incorrecta')
        return raw, info, data.get('device_sha256')
    except Exception:
        raise ValueError('Contraseña incorrecta o respaldo corrupto') from None


def private_write(path, raw):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    try:
        with os.fdopen(fd, 'wb') as f:
            f.write(raw)
            f.flush()
            os.fsync(f.fileno())
    except BaseException:
        path.unlink(missing_ok=True)
        raise


class Esptool:
    def __init__(self, port, baud=115200):
        if not port or port.startswith('-'):
            raise ValueError('Indica el puerto USB observado, no uno de ejemplo')
        self.port, self.baud = port, baud
        self.major = int(importlib.metadata.version('esptool').split('.')[0])
        if self.major not in (4, 5):
            raise ValueError('Verificar compatibilidad: esptool 4 o 5 requerido')

    def run(self, command, *args, before='default_reset', after='hard_reset'):
        if self.major == 5:
            command, before, after = [v.replace('_', '-') for v in (command, before, after)]
        cmd = [sys.executable, '-m', 'esptool', '--chip', 'esp32s3', '--port', self.port,
               '--baud', str(self.baud), '--before', before, '--after', after, command, *map(str, args)]
        # No shell, no --force, nunca erase_flash. Salida puede incluir identificadores privados.
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=900)
        if result.returncode:
            raise RuntimeError('esptool falló; revisa USB/puerto/versión. No se continúa ni se fuerza acceso.')
        return result.stdout

    def device_id(self):
        output = self.run('read_mac')
        macs = re.findall(r'(?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}', output)
        if not macs:
            raise RuntimeError('No se identificó el dispositivo; detener')
        return hashlib.sha256(macs[-1].lower().encode()).hexdigest()


def backup(tool, directory):
    """Dos lecturas sin arrancar la aplicación entre ellas; NVS no cambia."""
    first, second = Path(directory)/'read1.bin', Path(directory)/'read2.bin'
    device = tool.device_id()
    tool.run('read_flash', '0x0', '0x800000', first, after='no_reset')
    tool.run('read_flash', '0x0', '0x800000', second, before='no_reset')
    raw = first.read_bytes()
    if raw != second.read_bytes():
        raise ValueError('Las dos lecturas difieren: no se certifica esta copia')
    inspect_flash(raw)
    return raw, device


def main():
    os.umask(0o077)
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='cmd', required=True)
    b = sub.add_parser('backup', help='Leer dos veces por USB y guardar cifrado; no escribe flash')
    b.add_argument('--port', required=True)
    b.add_argument('--output', required=True)
    b.add_argument('--lights-safe', action='store_true', help='Confirmar iluminación independiente durante lectura')
    p = sub.add_parser('pack', help='Cifrar un dump existente sin contactar hardware')
    p.add_argument('input'); p.add_argument('--output', required=True)
    i = sub.add_parser('inspect', help='Validar respaldo cifrado, mostrar solo metadatos')
    i.add_argument('input')
    i.add_argument('--extract-spiffs', help='Extraer partición SPIFFS a archivo privado nuevo, sin montarla')
    r = sub.add_parser('restore', help='Plan por defecto; escribe solo con autorización explícita')
    r.add_argument('input'); r.add_argument('--port', required=True)
    r.add_argument('--execute', action='store_true')
    r.add_argument('--confirm-sha256')
    r.add_argument('--lights-safe', action='store_true')
    args = parser.parse_args()
    if args.cmd in ('backup', 'pack'):
        if Path(args.output).exists():
            raise ValueError('El destino existe; no se sobrescribe ningún respaldo')
        if args.cmd == 'backup' and not args.lights_safe:
            raise ValueError('La lectura reinicia/pausa ATOM: estabiliza luces y confirma --lights-safe')
        password = getpass.getpass('Contraseña nueva para cifrar: ')
        if getpass.getpass('Repite contraseña: ') != password:
            raise ValueError('Contraseñas distintas')
        if len(password) < 12:
            raise ValueError('Usa al menos 12 caracteres')
        with tempfile.TemporaryDirectory(prefix='cielo-private-') as temp:
            if args.cmd == 'backup':
                raw, device = backup(Esptool(args.port), temp)
            else:
                raw, device = Path(args.input).read_bytes(), None
            encrypted = seal(raw, password, device)
            check, info, _ = unseal(encrypted, password)
            if check != raw:
                raise ValueError('No se verifica el cifrado')
            private_write(args.output, encrypted)
            print(json.dumps(info, indent=2))
            print('Respaldo cifrado guardado. Las lecturas temporales se eliminan; no es borrado seguro del disco.')
            if args.cmd == 'pack':
                print('El dump original permanece privado en su ubicación; no se ha eliminado.')
        return
    raw, info, device = unseal(Path(args.input).read_bytes(), getpass.getpass('Contraseña del respaldo: '))
    print(json.dumps(info, indent=2))
    if args.cmd == 'inspect':
        if args.extract_spiffs:
            part = next((p for p in info['partitions'] if p['type']==1 and p['subtype']==0x82), None)
            if part is None:
                raise ValueError('Sin partición SPIFFS')
            private_write(args.extract_spiffs, raw[part['offset']:part['offset']+part['size']])
            print('SPIFFS extraída en privado. No se montó ni formateó el ATOM.')
        return
    print('Plan: restaurar imagen completa a 0x0 SOLO en el ESP32-S3 original; sobrescribe sus 8 MiB.')
    if not args.execute:
        print('No se ha contactado USB ni escrito flash. Requiere --execute --lights-safe --confirm-sha256 HASH.')
        return
    if not args.lights_safe or args.confirm_sha256 != info['sha256'] or not device:
        raise ValueError('Falta confirmación exacta, luces seguras o identidad del dispositivo original')
    tool = Esptool(args.port)
    if tool.device_id() != device:
        raise ValueError('No es el dispositivo original. Restauración cancelada.')
    with tempfile.TemporaryDirectory(prefix='cielo-private-') as temp:
        image = Path(temp)/'restore.bin'
        private_write(image, raw)
        # esptool conserva el encabezado original; no flags flash_mode/size/freq ni force.
        tool.run('write_flash', '0x0', image, after='no_reset')
        verify = Path(temp)/'verify.bin'
        tool.run('read_flash', '0x0', '0x800000', verify, before='no_reset')
        if verify.read_bytes() != raw:
            raise ValueError('Lectura posterior diferente: recuperación NO verificada, revisar con USB')
    print('Restauración verificada por lectura completa. Verifica después perfiles, acceso y luz física.')


if __name__ == '__main__':
    try:
        main()
    except Exception as e:
        print('Operación detenida: '+str(e), file=sys.stderr)
        sys.exit(1)
