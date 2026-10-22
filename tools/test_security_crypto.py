#!/usr/bin/env python3
"""Pruebas host: verificación ECDSA, sustitución de binario y KDF con sal distinta."""
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives import hashes,serialization
from cryptography.exceptions import InvalidSignature
from pathlib import Path
import hashlib,os
priv=ec.generate_private_key(ec.SECP256R1());pub=priv.public_key()
data=b'IWAGUMI CIELO 11.1 example bytes'*80
sig=priv.sign(data,ec.ECDSA(hashes.SHA256()))
pub.verify(sig,data,ec.ECDSA(hashes.SHA256()))
try:pub.verify(sig,data+b'cambiado',ec.ECDSA(hashes.SHA256()))
except InvalidSignature:pass
else:raise AssertionError('Se aceptó firmware modificado')
other=ec.generate_private_key(ec.SECP256R1())
try:other.public_key().verify(sig,data,ec.ECDSA(hashes.SHA256()))
except InvalidSignature:pass
else:raise AssertionError('Se aceptó firma de otro autor')
pw=b'una frase que el usuario no envia al chat'
s1,s2=os.urandom(16),os.urandom(16)
d1=hashlib.pbkdf2_hmac('sha256',pw,s1,60000,32)
d2=hashlib.pbkdf2_hmac('sha256',pw,s2,60000,32)
assert d1!=d2 and len(d1)==len(d2)==32
print('OK utilidades de firma opcional archivadas: ECDSA y PBKDF2 host; no son requisitos de LAN1')
