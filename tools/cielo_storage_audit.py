#!/usr/bin/env python3
"""Read-only audit of a LOCAL partition/full-flash copy. No USB, mount or repair."""
import argparse
import hashlib
import json
from pathlib import Path
from cielo_flash import inspect_flash

EXPECTED_OFFSET=0x670000
EXPECTED_SIZE=0x180000

def analyze(raw,full_flash=False):
    result={'schema':'cielo-storage-copy-audit-v1','read_only':True,
            'input_bytes':len(raw),'input_sha256':hashlib.sha256(raw).hexdigest(),
            'mount_attempted':False,'format_attempted':False,'repair_attempted':False}
    if full_flash:
        flash=inspect_flash(raw)
        parts=[p for p in flash['partitions'] if p['label']=='spiffs' and p['type']==1 and p['subtype']==0x82]
        if len(parts)!=1:raise ValueError('No hay una particion spiffs inequivoca')
        part=parts[0]
        if part['offset']!=EXPECTED_OFFSET or part['size']!=EXPECTED_SIZE:
            raise ValueError('Geometria distinta de la tabla CIELO auditada')
        if part['flags']&1:raise ValueError('Particion cifrada: no clasificar ciphertext como filesystem')
        result['partition_table_md5_verified']=flash['partition_table_md5_verified']
        result['nvs']=[p.get('nvs_inventory',{}) for p in flash['partitions'] if p['label']=='nvs']
        raw=raw[part['offset']:part['offset']+part['size']]
    elif len(raw)!=EXPECTED_SIZE:
        raise ValueError('La copia de particion debe tener exactamente 1572864 bytes; usa --full-flash para 8MiB')
    nonblank_count=len(raw)-raw.count(b'\xff')
    first=next((i for i,b in enumerate(raw) if b!=255),None)
    last=next((len(raw)-i-1 for i,b in enumerate(reversed(raw)) if b!=255),None)
    hints=[];start=0
    while len(hints)<8:
        at=raw.find(b'littlefs',start)
        if at<0:break
        hints.append(at);start=at+8
    result['partition']={'offset':EXPECTED_OFFSET,'bytes':len(raw),
        'sha256':hashlib.sha256(raw).hexdigest(),'all_bytes_ff':not nonblank_count,
        'non_ff_bytes':nonblank_count,'first_non_ff_offset':first,
        'last_non_ff_offset':last,
        'erased_4k_blocks':sum(raw[i:i+4096]==b'\xff'*4096 for i in range(0,len(raw),4096)),
        'total_4k_blocks':len(raw)//4096,'littlefs_text_hint_offsets':hints,
        'format':'BLANK' if not nonblank_count else 'UNDETERMINED',
        'signature_is_only_a_hint':True,'raw_distribution_is_not_live_filesystem_usage':True,
        'note':'Nonempty no demuestra corrupcion. Un texto littlefs no identifica de forma concluyente el formato.'}
    return result

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('image',type=Path);p.add_argument('--full-flash',action='store_true')
    p.add_argument('--compare',type=Path,help='Exigir que una segunda copia sea identica')
    a=p.parse_args();raw=a.image.read_bytes()
    if a.compare is not None and raw!=a.compare.read_bytes():
        raise ValueError('Las copias difieren: no se certifica la lectura')
    result=analyze(raw,a.full_flash);result['two_copies_match']=True if a.compare is not None else None
    print(json.dumps(result,indent=2,ensure_ascii=False))

if __name__=='__main__':main()
