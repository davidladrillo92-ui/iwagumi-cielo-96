"""Run the actual firmware saveProfile + NVS adapter with FS unavailable.

The allocation model is synthetic; it does not reconstruct the ATOM's pages.
"""
from pathlib import Path
import subprocess
import tempfile

src = Path('src/main.cpp').read_text()
adapter = src[src.index('struct CurveNvsStore {'):src.index('static String curveNvsSlotsJson')]
save = src[src.index('static bool saveProfile('):src.index('static bool positiveInt(')]
model = Path('tests/test_profile_transaction.cpp').read_text().split('int main(){', 1)[0]
shim = r'''
using esp_err_t=int;using nvs_handle_t=int;
constexpr int ESP_OK=Store::Ok,ESP_ERR_NVS_NOT_ENOUGH_SPACE=Store::NoSpace;
constexpr int ESP_ERR_INVALID_STATE=Store::Unsafe,ESP_ERR_INVALID_CRC=Store::Corrupt;
constexpr int ESP_ERR_NVS_NOT_FOUND=Store::Missing,ESP_ERR_INVALID_SIZE=0x104;
constexpr int ESP_ERR_INVALID_ARG=0x102,ESP_FAIL=Store::Io;
constexpr int NVS_READONLY=0,NVS_READWRITE=1;
struct nvs_stats_t{size_t free_entries=156;};
Store mock;int handles=0;bool statsFail=false;
int nvs_open(const char* ns,int,nvs_handle_t* h){assert(!strcmp(ns,"cielo96"));*h=++handles;return ESP_OK;}
void nvs_close(nvs_handle_t h){assert(h>0);}
int nvs_set_blob(nvs_handle_t h,const char* k,const void* p,size_t n){assert(h>0&&n==788);return mock.write(k,*static_cast<const ProfileImage*>(p));}
int nvs_get_blob(nvs_handle_t h,const char* k,void* p,size_t* n){assert(h>0&&*n==788);return mock.read(k,*static_cast<ProfileImage*>(p));}
int nvs_erase_key(nvs_handle_t h,const char* k){assert(h>0);return mock.erase(k);}
int nvs_commit(nvs_handle_t h){assert(h>0);return mock.commit();}
int nvs_get_stats(const char* p,nvs_stats_t*){assert(!strcmp(p,"nvs"));return statsFail?ESP_FAIL:ESP_OK;}
struct String {
 std::string value;
 String()=default;String(const char* p):value(p){};String(uint32_t n):value(std::to_string(n)){}
 friend String operator+(const String& a,const String& b){String s;s.value=a.value+b.value;return s;}
};
struct FsUnavailable {
 bool allow=true;bool allowNvsWrites(){return allow;}bool isLittlefs(){return false;}
 const char* backend(){return "NVS_LEGACY";}struct {uint32_t bytesWritten(){return 0;}}little;
 bool ready(){return false;}bool published(){return false;}bool uncertain(){return false;}
 bool save(const ProfileImage&){assert(false);return false;}
 bool newest(ProfileImage&){assert(false);return false;}
 const char* error(){return "NONE";}const char* slot(){return "NONE";}
} curveFs;
ProfileImage profile;bool profileRecovery=false,curveSaveInProgress=false;
const char* loadedCurveBackend="NVS_LEGACY";
profiletxn::Result lastCurveTransaction;
nvssafe::Failure lastCurveNvsFailure=nvssafe::Failure::None;
int lastCurveNvsErr=0,lastCurveNvsStatsErr=0;
const char* lastCurveNvsStage="sin_intento";const char* lastCurveNvsSlot="sin_intento";
size_t lastCurveNvsWritten=0,lastCurveNvsRequested=0,lastCurveNvsMinRequired=0,lastCurveNvsFreeBefore=0;
uint32_t curveNvsFailedAttempts=0,curveNvsSetAttempts=0;String lastBackupError;
void record(const char*,const String&){assert(!curveSaveInProgress||!lastCurveTransaction.verified);}
void reset(){mock=initial();profile=image(8);profileRecovery=false;statsFail=false;curveFs.allow=true;}
'''
checks = r'''
int main(){
 reset();const auto candidate=image(9,38),active=profile;
 assert(saveProfile(candidate));
 assert(profile.generation==9&&profile.pvCount==38&&profile.chCount==31);
 assert(lastCurveTransaction.verified&&lastCurveTransaction.inactiveReclaimed);
 assert(lastCurveNvsFreeBefore==156&&lastCurveNvsWritten==788);
 assert(!curveSaveInProgress&&mock.erased=="curvesA");
 assert(memcmp(&mock.records["curvesB"],&active,sizeof(active))==0);
 reset();mock.allWritesFull=true;
 assert(!saveProfile(candidate));assert(profile.generation==8);
 assert(mock.writes==2&&mock.eraseCalls==1&&!lastCurveTransaction.writeMayHavePersisted);
 assert(!curveSaveInProgress&&valid(mock,"curvesB"));
 reset();mock.records.erase("curvesA");mock.allWritesFull=true;
 assert(!saveProfile(candidate)&&mock.eraseCalls==0&&profile.generation==8);
 reset();mock.failRead=1;
 assert(!saveProfile(candidate)&&mock.writes==0&&mock.eraseCalls==0);
 reset();mock.failRead=4; // second protected read, immediately before erase
 assert(!saveProfile(candidate)&&mock.eraseCalls==0&&profile.generation==8);
 reset();mock.failCommit=2;
 assert(!saveProfile(candidate)&&lastCurveTransaction.writeAccepted);
 assert(lastCurveTransaction.writeMayHavePersisted&&profile.generation==8);
 assert(lastBackupError.value.find("no confirmado")!=std::string::npos);
 reset();statsFail=true;
 assert(!saveProfile(candidate)&&mock.writes==0&&mock.eraseCalls==0);
 reset();curveFs.allow=false;
 assert(!saveProfile(candidate)&&mock.writes==0&&mock.eraseCalls==0&&profile.generation==8);
 reset();mock.used={{50,50,50,50}};mock.publishedThenError=true;
 assert(!saveProfile(candidate)&&mock.records["curvesA"].generation==9);
 assert(lastCurveTransaction.writeMayHavePersisted&&!lastCurveTransaction.writeAccepted);
 assert(lastBackupError.value.find("no confirmado")!=std::string::npos);
 std::cout<<"PASS actual firmware saveProfile + NVS adapter: FS unavailable, 4357 fallback, single-copy/read/stats/commit errors and uncertain published write\n";
}
'''
with tempfile.TemporaryDirectory() as d:
    p=Path(d)/'test.cpp';p.write_text(model+shim+adapter+save+checks)
    binary=str(Path(d)/'test')
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-Iinclude',str(p),'-o',binary],check=True)
    subprocess.run([binary],check=True)
