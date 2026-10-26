from pathlib import Path
import subprocess,sys,tempfile,json,hashlib
sys.path.insert(0,'tools')
from cielo_storage_audit import analyze,EXPECTED_SIZE
blank=b'\xff'*EXPECTED_SIZE
a=analyze(blank);assert a['partition']['format']=='BLANK' and a['partition']['erased_4k_blocks']==384
content=bytearray(blank);content[-8:]=b'littlefs'
a=analyze(content);assert a['partition']['format']=='UNDETERMINED' and a['partition']['littlefs_text_hint_offsets']==[EXPECTED_SIZE-8]
assert not a['mount_attempted'] and not a['format_attempted'] and not a['repair_attempted']
try:analyze(b'wrong')
except ValueError:pass
else:raise AssertionError('Short image accepted')
with tempfile.TemporaryDirectory() as d:
    p=Path(d)/'private.bin';q=Path(d)/'read2.bin';p.write_bytes(content);q.write_bytes(content)
    before=hashlib.sha256(p.read_bytes()).hexdigest()
    r=subprocess.run([sys.executable,'tools/cielo_storage_audit.py',str(p),'--compare',str(q)],capture_output=True,text=True,check=True)
    assert json.loads(r.stdout)['two_copies_match'] and hashlib.sha256(p.read_bytes()).hexdigest()==before
    q.write_bytes(blank)
    r=subprocess.run([sys.executable,'tools/cielo_storage_audit.py',str(p),'--compare',str(q)],capture_output=True,text=True)
    assert r.returncode!=0 and hashlib.sha256(p.read_bytes()).hexdigest()==before
print('PASS local-copy audit: metadata only, blank/signature hint, exact size, identical copies, mismatch rejection and input bytes preserved')
