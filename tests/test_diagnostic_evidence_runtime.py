"""Execute the production JSON serializer with storage/radio-free session fixtures."""
from pathlib import Path
import json, subprocess, tempfile

source=Path('src/main.cpp').read_text()
checksum=source[source.index('static String profileWireChecksum(){'):source.index('static bool workLightPreparing=false;')]
effective=source[source.index(' const auto effectivePv=desiredPv();'):source.index(' j+=",\\\"pv_last_requested\\\":["')]
fixture=r'''
#include "cielo_diagnostic_evidence.h"
#include "cielo_cadence_diagnostics.h"
#include <string>
#include <iostream>
#include <type_traits>
struct String {
 std::string v;String()=default;String(const char*s):v(s){}String(const std::string&s):v(s){}
 template<class T,typename std::enable_if<std::is_integral<T>::value,int>::type=0>String(T n):v(std::to_string(n)){}
 void reserve(size_t n){v.reserve(n);}String&operator+=(const String&b){v+=b.v;return *this;}
 friend String operator+(String a,const String&b){return a+=b;}
};
String jsonQuote(const String&s){return String("\"")+s+String("\"");}
constexpr int ESP_OK=0;
uint32_t previousBootCount=141;
cielo96::ProfileImage profile;
const char* loadedCurveBackend="LITTLEFS_AB";
const char* lastCurveNvsStage="FS_VERIFICADO";
int lastCurveNvsErr=0;
uint32_t lastCurveNvsRequested=788,curveNvsFailedAttempts=0,cadenceSkippedBusy=355;
bool lastCurveRequestUnchanged=false;
struct {bool verified=false,writeMayHavePersisted=false;}lastCurveTransaction;
struct {bool mounted=true,pub=true,unsure=false;bool ready(){return mounted;}bool published(){return pub;}bool uncertain(){return unsure;}const char*backend(){return "LITTLEFS_AB";}}curveFs;
cielo96::cadence::Diagnostics cadenceDiagnostics;
cielo96::evidence::OperationalHistory operationalHistory;
bool pvOffObligation=true,pvControlDesired=true;
cielo::PvValue desiredPv(){return {58,78,132,1};}
'''
checks=r'''
int main(){
 profile.pvCount=profile.chCount=2;profile.pv[1].minute=profile.ch[1].minute=1440;
 profile.generation=14;cielo96::profileSeal(profile);
 cadenceDiagnostics.due=11694;cadenceDiagnostics.evaluated=11693;cadenceDiagnostics.inhibited=1;
 operationalHistory.capture("MANTENIMIENTO_INICIO",335763824,1793445698);
 for(int i=0;i<1000;i++)operationalHistory.capture("PV_POWER_OFF_GATT_OK",1,1);
 operationalHistory.capture("MANTENIMIENTO_FIN",340067200,1793450002);
 std::cout<<diagnosticEvidenceJson().v<<"\n";
 lastCurveNvsRequested=0;lastCurveNvsStage="ninguno";std::cout<<diagnosticEvidenceJson().v<<"\n";
 lastCurveRequestUnchanged=true;std::cout<<diagnosticEvidenceJson().v<<"\n";
 lastCurveRequestUnchanged=false;lastCurveNvsRequested=788;lastCurveNvsErr=-1;curveFs.unsure=true;
 std::cout<<diagnosticEvidenceJson().v<<"\n";
 curveFs.pub=curveFs.unsure=false;std::cout<<diagnosticEvidenceJson().v<<"\n";
 curveFs.mounted=false;loadedCurveBackend="NVS_LEGACY";lastCurveNvsErr=0;lastCurveTransaction.verified=true;lastCurveTransaction.writeMayHavePersisted=true;
 std::cout<<diagnosticEvidenceJson().v<<"\n";
 for(bool off:{true,false}){pvOffObligation=off;String j="{";
'''+effective+r'''
 std::cout<<(j+"}").v<<"\n";}
}
'''
with tempfile.TemporaryDirectory() as t:
    cpp=Path(t)/'diagnostic.cpp';exe=Path(t)/'diagnostic'
    cpp.write_text(fixture+checksum+'\n#include "cielo_diagnostic_json.inc"\n'+checks)
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-Iinclude',str(cpp),'-o',str(exe)],check=True)
    # Effective-PV fragments begin with a comma because they append to an existing object.
    lines=subprocess.check_output([str(exe)],text=True).splitlines()
    data=[json.loads(s.replace('{,','{',1)) for s in lines]
first=data[0]
assert first['curve_save']['verified'] is True and first['curve_save']['last_result']=='VERIFIED'
assert first['profile']['content_valid'] and first['profile']['generation']==14
assert first['cadence']['busy_basis_points']==303 and first['cadence']['missed_updates'] is None
assert first['operational_history']['samples']==2 and not first['optically_verified']
assert data[1]['curve_save']['verified'] is None
assert data[2]['curve_save']['last_result']=='UNCHANGED' and data[2]['curve_save']['verified']
assert data[3]['curve_save']['last_result']=='UNCERTAIN' and not data[3]['curve_save']['verified']
assert data[4]['curve_save']['last_result']=='FAILED'
assert data[5]['curve_save']['backend']=='NVS_LEGACY' and data[5]['curve_save']['verified']
assert data[6]['pv_effective_requested']==[0,0,0,0]
assert data[7]['pv_effective_requested']==[58,78,132,1]
assert source.count('"diagnostic_evidence"')==0  # C++ keys are escaped.
assert source.count('\\"diagnostic_evidence\\"')==2
assert '"MANTENIMIENTO_COMPROBANDO"' in Path('include/cielo_diagnostic_evidence.h').read_text()
print('PASS production diagnostics JSON: FS/NVS, unknown/no-op/failure/uncertainty, profile checksum, busy denominator, retained maintenance, effective OFF')
