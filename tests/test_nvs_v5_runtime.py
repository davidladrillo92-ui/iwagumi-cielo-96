"""Round-trip the user's exact V5 through the production transaction."""
from pathlib import Path
import json,subprocess,tempfile
new=json.loads(Path('tests/fixtures/v5_nvs_regression.json').read_text())
old=json.loads(Path('reference/CIELO_V5_LUNA_DIRECTA_PERFIL_V2_VERIFICADO.json').read_text())
assert new['lights']['chihiros']['points']==old['lights']['chihiros']['points']
pv=new['lights']['pezverde']['points'];ch=new['lights']['chihiros']['points']
wire=lambda rows:';'.join(','.join(map(str,r)) for r in rows)
h=2166136261
for b in (wire(pv)+'|'+wire(ch)).encode():h=((h^b)*16777619)&0xffffffff
assert format(h,'08x')==new['integrity']['checksum']=='c4d2d529'
base=Path('tests/test_profile_transaction.cpp').read_text().split('int main(){',1)[0]
def fill(name,d,generation):
    p=d['lights']['pezverde']['points'];c=d['lights']['chihiros']['points']
    code='ProfileImage '+name+'{};'+name+'.generation='+str(generation)+';'+name+'.pvCount='+str(len(p))+';'+name+'.chCount='+str(len(c))+';\n'
    for i,r in enumerate(p):code+=name+'.pv['+str(i)+']={'+','.join(map(str,r))+'};\n'
    for i,r in enumerate(c):code+=name+'.ch['+str(i)+']={'+','.join(map(str,r))+'};\n'
    return code+'profileSeal('+name+');assert(curves::curveValid('+name+'));\n'
source=base+'int main(){\n'+fill('active',old,8)+fill('candidate',new,9)+r'''
 auto s=initial();s.records["curvesB"]=active;s.records["curvesA"]=active;s.records["curvesA"].generation=7;profileSeal(s.records["curvesA"]);
 const auto r=profiletxn::save(s,"curvesA","curvesB",active,candidate,true);
 assert(r.verified&&r.inactiveReclaimed&&r.initialWriteError==Store::NoSpace);
 assert(memcmp(&s.records["curvesA"],&candidate,sizeof(candidate))==0);
 assert(memcmp(&s.records["curvesB"],&active,sizeof(active))==0);
 // Model a cold boot selecting the newest independently validated NVS image.
 const auto* loaded=curves::newestValid(&s.records["curvesA"],&s.records["curvesB"]);
 assert(loaded&&loaded->generation==9&&loaded->pvCount==38&&loaded->chCount==31);
 assert(memcmp(loaded->pv,candidate.pv,sizeof(candidate.pv))==0);
 assert(memcmp(loaded->ch,candidate.ch,sizeof(candidate.ch))==0);
 assert(memcmp(loaded->ch,active.ch,sizeof(active.ch))==0);
}
'''
with tempfile.TemporaryDirectory() as d:
    p=Path(d)/'test.cpp';p.write_text(source);exe=str(Path(d)/'test')
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-Iinclude',str(p),'-o',exe],check=True)
    subprocess.run([exe],check=True)
print('PASS exact V5 c4d2d529: 38/31, unchanged Chihiros, protected generation 8, validated generation 9 readback/boot')
