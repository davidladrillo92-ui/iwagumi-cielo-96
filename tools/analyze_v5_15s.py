#!/usr/bin/env python3
"""Modelo SIN RADIO: cuenta órdenes RGB distintas de V5 al muestrear cada 15/30 s.
No pretende simular latencia real, consumo de RAM ESP32 ni confirmacion fisica.
"""
import json
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'tests/fixtures/iwagumi_v5_luna_directa.json'
data=json.loads(p.read_text())['lights']
def evaluate(points,sec,seconds_mode):
 t=sec if seconds_mode else sec//60*60
 if t<points[0][0]*60:return tuple(0 for _ in points[0][1:])
 if t>=points[-1][0]*60:return tuple(points[-1][1:])
 lo=next(i for i in range(len(points)-1) if points[i][0]*60<=t<points[i+1][0]*60)
 a,b=points[lo:lo+2]
 if len(a)==4 and a==[1379,2,2,2] and b==[1380,1,1,2]:return tuple(a[1:])
 dt=t-a[0]*60;D=(b[0]-a[0])*60
 # C++ cielomix: symmetrical round-to-nearest, not bankers rounding
 def mix(x,y):
  delta=y-x
  offset=(abs(delta)*dt+D//2)//D
  return x+(offset if delta>=0 else -offset)
 return tuple(mix(x,y) for x,y in zip(a[1:],b[1:]))
def simulate(light,start,end,interval,seconds_mode):
 arr=data[light]['points'];prev=None;changed=0
 for sec in range(start*60,end*60+1,interval):
  out=evaluate(arr,sec,seconds_mode)
  if out!=prev:changed+=1;prev=out
 return changed
print('MODELADO de cambios RGB potenciales (sin trafico GATT real ni HEAP):')
for start,end,title in [(870,940,'Amanecer 14:30-15:40'),(1310,1440,'Atardecer 21:50-24:00')]:
 for light in ('pezverde','chihiros'):
  values=[simulate(light,start,end,interval,interval==15) for interval in (30,15)]
  print(title,light, '30s=',values[0], '15s=',values[1], 'cambio=',values[1]-values[0])
chi=data['chihiros']['points']
assert [v for v in chi if v[0] in (1379,1380)]==[[1379,2,2,2],[1380,1,1,2]]
assert evaluate(chi,1379*60+45,True)==(2,2,2)
assert evaluate(chi,1380*60,True)==(1,1,2)
print('OK: regresión lunar V5 y modelo de carga')
