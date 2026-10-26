"""Compile and exercise RECOVERY3.4 NVS decisions without touching hardware."""
from pathlib import Path
import subprocess
import tempfile

source = Path("src/main.cpp").read_text()
assert '#include "cielo_nvs_safe.h"' in source
assert "nvs_set_blob(handle,key,&candidate,sizeof(candidate))" in source
assert "nvs_commit(handle)" in source
assert "nvs_get_blob(verify,key,&readback,&bytes)" in source
assert "lastCurveNvsErr=err" in source
assert "nvsPreserveCurveHeadroom()" in source
assert 'previous_profile_preserved' in source
assert "if(curveSaveInProgress)" in source
assert "nvs_get_stats(\"nvs\",&stats)" in source
assert "ESP_ERR_NVS_NOT_ENOUGH_SPACE" in source
assert "prefs.clear()" not in source
assert "nvs_erase_all(" not in source
assert "nvs_erase_key(" not in source

cpp = r"""
#include <cassert>
#include <cstring>
#include "cielo_nvs_safe.h"
int main() {
 using namespace cielo96::nvssafe;
 assert(target(false,0,false,0)[6]=='A');
 assert(target(true,8,false,0)[6]=='B');
 assert(target(false,0,true,9)[6]=='A');
 assert(target(true,8,true,7)[6]=='B');
 assert(target(true,7,true,8)[6]=='A');
 assert(target(true,0xffffffffu,true,0)[6]=='A');
 assert(target(true,0,true,0xffffffffu)[6]=='B');
 assert(targetPreservesOnlyCopy(true,false,"curvesB"));
 assert(!targetPreservesOnlyCopy(true,false,"curvesA"));
 assert(targetPreservesOnlyCopy(false,true,"curvesA"));
 assert(!targetPreservesOnlyCopy(false,true,"curvesB"));
 assert(minimumFreeEntries(1)==3);
 assert(minimumFreeEntries(32)==3);
 assert(minimumFreeEntries(33)==4);
 assert(definitelyTooLittleSpace(3,33));
 assert(!definitelyTooLittleSpace(4,33));
 assert(std::strstr(message(Failure::InsufficientEntries),"insuficiente")!=nullptr);
 assert(std::strstr(message(Failure::WriteFailed),"escritura")!=nullptr);
 assert(std::strstr(message(Failure::CommitFailed),"confirmar")!=nullptr);
 assert(std::strstr(message(Failure::VerificationFailed),"verificacion")!=nullptr);
}
"""
with tempfile.TemporaryDirectory() as d:
    src = Path(d) / "test.cpp"
    exe = Path(d) / "test"
    src.write_text(cpp)
    subprocess.run(
        ["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
         "-Iinclude", str(src), "-o", str(exe)],
        check=True,
    )
    subprocess.run([str(exe)], check=True)
print("PASS NVS preflight, inactive-slot preservation, generation rollover, error taxonomy")
