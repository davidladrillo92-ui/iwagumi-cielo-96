"""Execute the actual firmware Wi-Fi HTTP/boot handlers with real record/NVS adapter and synthetic IO."""
from pathlib import Path
import subprocess,tempfile,json
src=Path('src/main.cpp').read_text()
handlers=src[src.index('static void applyWifiImage('):src.index('static void configureRoutes(){')]
diag=src[src.index('static String wifiConfigJson(){'):src.index('static bool currentCritical(){')]
fixture=r'''
#include "cielo_wifi_nvs.h"
#include <iostream>
#include <type_traits>
struct String{
 std::string v;String()=default;String(const char*s):v(s){}String(const std::string&s):v(s){}
 template<class T,typename std::enable_if<std::is_integral<T>::value,int>::type=0>String(T n):v(std::to_string(n)){}
 const char*c_str()const{return v.c_str();}size_t length()const{return v.size();}bool isEmpty()const{return v.empty();}
 String&operator+=(const String&b){v+=b.v;return *this;}
 friend String operator+(String a,const String&b){return a+=b;}
 bool operator!=(const String&b)const{return v!=b.v;}
 bool operator==(const String&b)const{return v==b.v;}
};
String jsonQuote(const String&s){return String("\"")+s+String("\"");}
std::map<std::string,std::map<std::string,String>> legacyData;
struct Preferences{
 std::string ns="cielo96";
 bool begin(const char*n,bool readonly){assert(readonly);ns=n;return true;}
 String getString(const char*k,const char*f){auto i=legacyData[ns].find(k);return i==legacyData[ns].end()?String(f):i->second;}
 void end(){}
}prefs; // No mutation APIs: legacy writes fail to compile.
#define CIELO_WIFI_SSID "factory-network"
#define CIELO_WIFI_PASSWORD "factory-password"
struct {std::map<std::string,String> args;String arg(const char*k){return args[k];}}srv;
int http=0;String body;void fail(int code,const String&message){http=code;body=message;}
void jsonSend(int code,const String&message){http=code;body=message;}
std::vector<String> events;void record(const String&a,const String&b){events.push_back(a+":"+b);}
enum class OtaPhase{Idle,Ready};bool otaBusy=false;struct {OtaPhase phase=OtaPhase::Idle;}otaTxn;
bool workLightPreparing=false;struct {bool on=false;bool active(){return on;}}workLight;
struct {int disconnects=0;void disconnect(bool off,bool erase){assert(!off&&!erase);disconnects++;}}WiFi;
uint32_t millis(){return 100000;}uint32_t lastWiFiAttempt=0;
String storedSSID="",storedPass="";
static cielo96::wificfg::Selection wifiConfigSelection;
static cielo96::wificfg::Result wifiConfigLastSave;
static bool wifiRamStorageVerified=true;
static uint32_t wifiConfigWriteAttempts=0,wifiConfigVerifiedSaves=0,wifiConfigUnchangedSaves=0;
'''
checks=r'''
int main(){
 using namespace cielo96::wificfg;using namespace mocknvs;
 reset();legacyData["cielo96"]["ssid"]="first-network";legacyData["cielo96"]["wifi_pw"]="first-password";
 const auto legacyBefore=legacyData;loadWifiConfiguration();assert(storedSSID=="first-network"&&storedPass=="first-password"&&writes==0);
 srv.args["ssid"]="new-network";srv.args["password"]="new-password";
 // All protected modes reject before touching persistence or live Wi-Fi.
 otaBusy=true;configureWifi();assert(http==423&&writes==0);otaBusy=false;
 otaTxn.phase=OtaPhase::Ready;configureWifi();assert(http==423);otaTxn.phase=OtaPhase::Idle;
 workLight.on=true;configureWifi();assert(http==409);workLight.on=false;
 workLightPreparing=true;disableWifi();assert(http==409);workLightPreparing=false;
 srv.args["password"]="bad";configureWifi();assert(http==400&&writes==0&&WiFi.disconnects==0);srv.args["password"]="new-password";
 writeError=ESP_ERR_NVS_NOT_ENOUGH_SPACE;configureWifi();assert(http==507&&WiFi.disconnects==0&&storedSSID=="first-network"&&storedPass=="first-password");
 writeError=0;commitError=-10;configureWifi();assert(http==500&&wifiConfigLastSave.mayHavePersisted&&WiFi.disconnects==0&&storedSSID=="first-network");
 // Commit error may already publish. Retry verifies exact blob, changes RAM only then.
 commitError=0;configureWifi();assert(http==200&&storedSSID=="new-network"&&storedPass=="new-password"&&WiFi.disconnects==1);
 const int before=writes,connections=WiFi.disconnects;configureWifi();assert(http==200&&writes==before&&WiFi.disconnects==connections);
 assert(wifiConfigSelection.degraded); // First atomic record, only one valid copy.
 disableWifi();assert(http==200&&storedSSID.isEmpty()&&storedPass.isEmpty()&&WiFi.disconnects==connections+1&&!wifiConfigSelection.degraded);
 storedSSID="stale";storedPass="stale-password";loadWifiConfiguration();assert(storedSSID.isEmpty()&&storedPass.isEmpty()); // Tombstone beats legacy and defaults.
 const int offWrites=writes;disableWifi();assert(http==200&&writes==offWrites);
 assert(legacyData==legacyBefore);
 // Last-save diagnostics include metadata only, no network names/passwords.
 auto json=wifiConfigJson();assert(json.v.find("new-password")==std::string::npos&&json.v.find("new-network")==std::string::npos);
 for(const auto&e:events)assert(e.v.find("password")==std::string::npos&&e.v.find("network")==std::string::npos);
 std::cout<<json.v<<"\n";
 // A damaged/unknown atomic record cannot resurrect old credentials.
 data["wifiB"][0]^=1;loadWifiConfiguration();assert(wifiConfigSelection.origin==Origin::Blocked&&storedSSID.isEmpty());
 reset();legacyData["cielo96"]["ssid"]="";legacyData["cielo96"]["wifi_pw"]="";
 legacyData["iwagumi-rgb"]["homeSsid"]="old-network";legacyData["iwagumi-rgb"]["homePass"]="old-password";
 loadWifiConfiguration();assert(storedSSID=="old-network"&&storedPass=="old-password"&&writes==0);
 // Invalid legacy pair must not become a partial live credential pair.
 legacyData["iwagumi-rgb"]["homePass"]="short";loadWifiConfiguration();assert(storedSSID.isEmpty()&&writes==0);
 reset();legacyData.clear();loadWifiConfiguration();assert(storedSSID=="factory-network"&&storedPass=="factory-password"&&writes==0);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.cpp';p.write_text(fixture+diag+handlers+checks);exe=str(Path(d)/'test')
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-Itests/mockwifi','-Iinclude',str(p),'-o',exe],check=True)
 result=json.loads(subprocess.check_output([exe],text=True))
 assert result['backend']=='NVS_WIFI_AB' and result['record_bytes']==112
 assert result['credentials_redacted'] and result['legacy_keys_preserved'] and not result['enabled']
 assert result['sdk_storage_ram_verified'] and result['unchanged_saves']>=2
setup=src[src.index('void setup(){'):src.index('void loop(){')]
assert setup.index('WiFi.persistent(false)')<setup.index('WiFi.mode(')<setup.index('WiFi.softAP(')<setup.index('WiFi.begin(')
assert 'WiFi.mode(' not in setup[:setup.index('WiFi.persistent(false)')]
assert 'prefs.put' not in handlers and 'prefs.remove' not in handlers
for token in ['nvs_erase','nvs_flash_init','nvs_flash_erase','format(','clear(']:
 assert token not in Path('include/cielo_wifi_nvs.h').read_text()
print('PASS actual Wi-Fi handlers: legacy read-only, atomic verify before RAM/disconnect, failures, unchanged requests, tombstone, all mode guards, redaction and RAM SDK initialization order')
