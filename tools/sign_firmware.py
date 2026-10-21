#!/usr/bin/env python3
"""Firma por separado BIN exactos para CIELO: ECDSA P-256, SHA-256, DER hex.
La clave privada jamás debe subirse al repositorio ni entrar en el firmware.
"""
import argparse, hashlib, pathlib
from cryptography.hazmat.primitives.serialization import load_pem_private_key
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives import hashes
from cryptography.exceptions import InvalidSignature
p=argparse.ArgumentParser();p.add_argument('firmware',type=pathlib.Path);p.add_argument('--private-key',required=True,type=pathlib.Path);p.add_argument('--output',type=pathlib.Path)
a=p.parse_args()
key=load_pem_private_key(a.private_key.read_bytes(),password=None)
if not isinstance(key,ec.EllipticCurvePrivateKey) or not isinstance(key.curve,ec.SECP256R1):raise SystemExit('Se requiere clave P-256')
b=a.firmware.read_bytes()
if not b or len(b)>0x330000:raise SystemExit('BIN vacio o mayor que particion')
sig=key.sign(b,ec.ECDSA(hashes.SHA256()))
try:key.public_key().verify(sig,b,ec.ECDSA(hashes.SHA256()))
except InvalidSignature:raise SystemExit('Verificacion de firma fallida')
target=a.output or a.firmware.with_suffix(a.firmware.suffix+'.sig')
target.write_text(sig.hex()+'\n')
print('FIRMADO',a.firmware.name,'SHA256',hashlib.sha256(b).hexdigest(),'firma',target.name,'bytes',len(sig))
