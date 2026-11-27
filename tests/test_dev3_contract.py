"""Verify exact DEV2 protected bytes, scope, and compile extracted runtime trend serializer."""
from pathlib import Path
import json,hashlib,subprocess,tempfile
s=Path('src/main.cpp').read_text();fixture=json.loads(Path('tests/fixtures/dev2_protected_sha256.json').read_text())
excepted={'include/cielo_history314.h','src/cielo_ui.cpp','web/interfaz_r4.html'}
for name,digest in fixture.items():
 if name not in excepted:assert hashlib.sha256(Path(name).read_bytes()).hexdigest()==digest,name
assert len(fixture)==67
report=s[s.index('static void diagnosticJson(){'):s.index('static void submitPv',s.index('static void diagnosticJson(){'))]
assert 'reportClockTrusted' not in s[:s.index('static void diagnosticJson(){')]
assert 'report_id' not in s[:s.index('static void diagnosticJson(){')]
assert 'reserve(28000)' not in report and 'cieloTrendJson(j)' in report
assert 'report_crc32' in report and report.index('report_crc32')<report.index('if(j.flush())')
assert 'srv.setContentLength(CONTENT_LENGTH_UNKNOWN)' in report and 'srv.sendContent("")' in report
assert 'reportClockTrusted?dateTimeAt(reportEpoch)' in report and 'reportClockTrusted?String(reportEpoch)' in report
assert 'historyStore315.load(dailyHistory314)' in s
checkpoint=s[s.index('// Optional history writes'):s.index('profileDeliveryTick314();',s.index('// Optional history writes'))]
for token in ('curveFs.writable()', '!otaCandidateBoot','!rescueOpen','!workLightPreparing','!queue.busy()', '!pvDisconnectInterlock','PvSafetyState::OffGattOk','pvOffNextTry-millis()', '65536','!isolated315[0].waiting','historyStore315.due','historyStore315.checkpoint'):
 assert token in checkpoint,token
assert 'prefs.' not in checkpoint and 'format' not in checkpoint
fn=s[s.index('template<class Output> static void cieloTrendJson'):s.index('static bool lampControlCritical',s.index('template<class Output> static void cieloTrendJson'))]
mock=r'''
#include <string>
#include <iostream>
#include <cassert>
#include "cielo_history315.h"
#include "cielo_report315.h"
struct String {
 std::string s; String(){} String(const char* p):s(p){} String(char c):s(1,c){} String(uint32_t n):s(std::to_string(n)){} String(uint8_t n):s(std::to_string(n)){}
 bool reserve(size_t n){s.reserve(n);return true;}size_t length()const{return s.size();}const char* c_str()const{return s.c_str();}void remove(size_t){s.clear();}bool concat(const String&v){s+=v.s;return true;}
};
String operator+(const String&a,const String&b){String x;x.s=a.s+b.s;return x;}
String jsonQuote(const String&s){return String("\"")+s+"\"";}
struct CieloTrendSample {uint32_t epoch=0,pv=0,ch=0,pvErrors=0,chErrors=0,heap=0;};
static CieloTrendSample cieloTrend[96]{};uint32_t cieloDaySamples=0,cieloNightSamples=96,historyRejectedSamples315=0;uint8_t cieloTrendCount=96,cieloTrendHead=0;
static cielo96::history314::Ring dailyHistory314;
static cielo96::history315::Posix io;static cielo96::history315::Store<cielo96::history315::Posix> historyStore315(io);
struct Sink {std::string text;bool send(const String&v){text+=v.s;return true;}};
'''
main=r'''
int main(){for(unsigned i=0;i<96;++i){cieloTrend[i].epoch=1795700000+i;cieloTrend[i].pv=i;cieloTrend[i].ch=2*i;cieloTrend[i].heap=99000;}
 for(unsigned i=0;i<30;++i)dailyHistory314.sample(2026300+i,10,20,1,2,90000-i);
 Sink sink;cielo96::report315::Writer<String,Sink> j(sink);assert(j.begin());cieloTrendJson(j);assert(j.flush());assert(j.peak()<=4096);std::cout<<sink.text;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.cpp';p.write_text(mock+fn+main);exe=Path(d)/'test';subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror','-Iinclude',str(p),'-o',str(exe)],check=True);out=json.loads(subprocess.check_output([str(exe)]))
assert len(out['history'])==96 and len(out['daily_30']['days'])==30
assert out['daily_30']['days'][-1]==[2026329,1,10,20,1,2,89971]
assert out['daily_30']['persisted'] is False and out['daily_30']['checkpoint_epoch'] is None
print('PASS exact 64 protected DEV2 files, real trend serializer 96+30, storage and report wiring')
