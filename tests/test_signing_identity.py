from pathlib import Path
import tempfile,subprocess,sys
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives import serialization
with tempfile.TemporaryDirectory() as temp:
 p=Path(temp);key=ec.generate_private_key(ec.SECP256R1())
 (p/'test.pem').write_bytes(key.private_bytes(serialization.Encoding.PEM,serialization.PrivateFormat.PKCS8,serialization.NoEncryption()))
 (p/'test.bin').write_bytes(b'example'*100)
 r=subprocess.run([sys.executable,'tools/sign_firmware.py',str(p/'test.bin'),'--private-key',str(p/'test.pem')],capture_output=True,text=True)
 assert r.returncode!=0 and 'no corresponde' in r.stderr
 assert not (p/'test.bin.sig').exists()
print('PASS: signing refuses a private key different from the immutable original identity')
