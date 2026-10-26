"""Host-side recovery policy edge cases; no physical light manipulation."""
from pathlib import Path
import subprocess
import tempfile

source = Path("src/main.cpp").read_text()
assert "cielo96::pvday::can_resume(" in source
assert "cielo96::pvday::prioritize_boot_day(" in source
assert "cielo96::autonomy::Retry chReconnect" in source
assert "cieloRequestReturn(" in source
assert "cieloChAwaitResponse" in source
assert "cielo96::ble_link_status(1)" in source

cpp = r"""
#include <cassert>
#include "cielo_autonomy_policy.h"
#include "cielo_pv_day_recovery.h"
#include "cielo_ble_presence.h"
int main() {
 using namespace cielo96;
 pvday::Input safe{true,true,true,true,true,true,false,true,25};
 assert(pvday::can_resume(safe));
 safe.off_gatt=false;assert(!pvday::can_resume(safe));safe.off_gatt=true;
 safe.clock_ok=false;assert(!pvday::can_resume(safe));safe.clock_ok=true;
 safe.worker_alive=false;assert(!pvday::can_resume(safe));safe.worker_alive=true;
 safe.current_brightness=0;assert(!pvday::can_resume(safe));
 assert(pvday::early_boot_ready(true,false,45000,true,true,false,false));
 assert(!pvday::early_boot_ready(true,true,45000,true,true,false,false));
 assert(pvday::prioritize_boot_day(true,true,true,true,false,50000));
 assert(!pvday::prioritize_boot_day(true,true,false,true,false,50000));
 assert(pvday::defer_ch_return(true,19999));
 assert(!pvday::defer_ch_return(true,20000));
 autonomy::Retry ch{};
 ch.arm(1000);assert(ch.recovering&&ch.pending);
 assert(!ch.ready(30000,true));
 assert(ch.ready(31000,true));
 assert(!ch.ready(31000,false));
 ch.begin();assert(!ch.pending&&ch.attempts==1);
 ch.success();assert(!ch.recovering&&ch.episodes==0);
 assert(presence::recent(true,1000,91000,0));
 assert(!presence::recent(false,1000,91000,0));
 assert(!presence::recent(true,1000,91001,0));
 assert(!presence::recent(true,1000,91000,1));
}
"""
with tempfile.TemporaryDirectory() as d:
    source_file = Path(d) / "policy.cpp"
    binary = Path(d) / "policy"
    source_file.write_text(cpp)
    subprocess.run(
        ["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
         "-Iinclude", str(source_file), "-o", str(binary)],
        check=True,
    )
    subprocess.run([str(binary)], check=True)
print("PASS Chihiros retry and Pez Verde daytime recovery safety policy")
