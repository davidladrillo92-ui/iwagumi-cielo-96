#!/usr/bin/env python3
from pathlib import Path
s=Path('src/main.cpp').read_text();html=Path('web/interfaz_r4.html').read_text()
engine=Path('include/cielo_engine.h').read_text();profile=Path('include/cielo_profile_store.h').read_text()
assert 'cielo::atSecond(profile.pv' in s and 'cielo::atSecond(profile.ch' in s
assert 'uint32_t(s' not in s
assert 'if(cieloCurveIntervalSec==15' in s and 'CURVA_15S_AUTO_30S' in s
assert 'cieloCurveIntervalSec=30; // 15s es un ensayo TEMPORAL' in s
assert 'queue.metrics().soft_timeouts' in s and 'curveTrialFallbacks' in s
assert s.count('srv.on("/api/profile/transaction",HTTP_POST')==1
assert 'if(!parsePoints(pv,true,candidate) || !parsePoints(ch,false,candidate))' in s
assert 'if(generation!=profile.generation)' in s
assert 'if(!saveProfile(candidate))' in s
assert 'curveCrc32(data)' in s and 'crc32Ascii(curvePayload(' in html
assert 'uint16_t pvCount' in profile and 'cielo::ChiPoint ch[64]' in profile
assert 'curvesA' in s and 'curvesB' in s and 'profile=candidate' in s
assert '/api/save' in s and 'fail(410,"Usa guardado conjunto /api/profile/transaction")' in s
assert 'function curveHistory(' in html and 'function curveRemember(' in html
assert 'schema_version:2' in html and "timezone:'Europe/Madrid'" in html
assert 'if(obj.checksum?.algorithm' in html
assert 'a.length>64' in html and 'max=64' in html
assert 'atSecond(const PvPoint*' in engine and 'atSecond(const ChiPoint*' in engine
assert 'b.r==1 && b.g==1 && b.b==2' in engine
assert 'const uint32_t curveTickMs=uint32_t(cieloCurveIntervalSec)*1000UL;' in s
assert 'queue.pending(cielo::ble::Device::PezVerde)' not in s[s.index('const bool chWindow=true;'):s.index('static void bleTick()')]
assert 'queue.pending(cielo::ble::Device::Chihiros)' not in s[s.index('const bool chWindow=true;'):s.index('static void bleTick()')]
assert 'CIELO_VERSION="11.1.0-RC5-B1B5B3-B2B4-PERFILES"' in Path('platformio.ini').read_text()
print('B2+B4 source contract PASS')
