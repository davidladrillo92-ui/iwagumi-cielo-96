#!/usr/bin/env python3
"""Prepare named screenshots and a small index; full evidence stays in xcresult."""
import hashlib,json,re,shutil,sys
from pathlib import Path
source=Path(sys.argv[1]).resolve();output=Path(sys.argv[2]);output.mkdir(parents=True,exist_ok=True)
manifest=json.loads((source/'manifest.json').read_text())
index=[]
for test in manifest:
    for attachment in test.get('attachments',[]):
        item=(source/attachment['exportedFileName']).resolve()
        if not item.is_relative_to(source) or item.suffix.lower() not in ('.png','.txt'):
            continue
        human=attachment.get('suggestedHumanReadableName',item.name)
        short=human.split('_0_')[0] if '_0_' in human else Path(human).stem
        if item.suffix.lower()=='.txt':short=test.get('testIdentifier','test')+'_'+short
        name=re.sub(r'[^A-Za-z0-9_.-]','_',short)+item.suffix.lower()
        target=output/name
        if target.exists():target=output/(target.stem+'_'+item.stem+target.suffix)
        shutil.copyfile(item,target)
        index.append({'test':test.get('testIdentifier'),'file':target.name,'sha256':hashlib.sha256(target.read_bytes()).hexdigest()})
(output/'INDICE.json').write_text(json.dumps(index,ensure_ascii=False,indent=2)+'\n')
print(f'Prepared {len(index)} screenshot/text attachments; recordings remain in full xcresult.')
