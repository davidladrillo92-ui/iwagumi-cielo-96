"""Compare actual JS preview against the unchanged C++ engine at every minute."""
from pathlib import Path
import json,subprocess,tempfile
p=json.loads(Path('tests/fixtures/v5_nvs_regression.json').read_text())['lights']
pv=p['pezverde']['points'];ch=p['chihiros']['points']
cpp='#include "cielo_engine.h"\n#include <iostream>\nint main(){\n'
cpp+='cielo::PvPoint pv[]={'+','.join('{'+','.join(map(str,p))+'}' for p in pv)+'};\n'
cpp+='cielo::ChiPoint ch[]={'+','.join('{'+','.join(map(str,p))+'}' for p in ch)+'};\n'
cpp+='for(int t=0;t<=1440;t++){auto a=cielo::at(pv,sizeof(pv)/sizeof(*pv),t);auto b=cielo::at(ch,sizeof(ch)/sizeof(*ch),t);std::cout<<int(a.r)<<","<<int(a.g)<<","<<int(a.b)<<","<<int(a.brightness)<<","<<int(b.r)<<","<<int(b.g)<<","<<int(b.b)<<"\\n";}}'
s=Path('web/interfaz_r4.html').read_text();js=s[s.index('function vAt('):s.index('function scene(')]
js+='\nconst pv='+json.dumps(pv)+',ch='+json.dumps(ch)+';for(let t=0;t<=1440;t++)console.log([...vAt(pv,t),...vAt(ch,t)].join(","));'
with tempfile.TemporaryDirectory() as d:
 d=Path(d);(d/'main.cpp').write_text(cpp);(d/'check.js').write_text(js)
 subprocess.run(['g++','-std=c++17','-Iinclude',str(d/'main.cpp'),'-o',str(d/'check')],check=True)
 a=subprocess.check_output([str(d/'check')],text=True);b=subprocess.check_output(['node',str(d/'check.js')],text=True)
 assert a==b,'JS preview differs from the actual engine'
print('PASS real JS/C++ equivalence: both lamps,1441 minutes, descending half-steps and midnight RGB endpoint')
