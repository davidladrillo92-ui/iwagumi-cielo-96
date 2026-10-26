"""RECOVERY3.5 static safety assertions: not a replacement for device trials."""
from pathlib import Path
src = Path("src/main.cpp").read_text(encoding="utf8")
fs = Path("include/cielo_profile_fs.h").read_text(encoding="utf8")
part = Path("partitions_8mb.csv").read_text(encoding="utf8")
policy = Path("include/cielo_curve_policy.h").read_text(encoding="utf8")
assert '#include "cielo_profile_fs.h"' in src
assert "if(curveFs.begin())" in src
assert "if(curveFs.ready())" in src
assert "curveFs.save(candidate)" in src
assert "curveFs.newest(file)" in src
assert "curveFs.newest(fsVerified)" in src
assert "memcmp(&fsVerified,&candidate,sizeof(candidate))!=0" in src
assert "nvs_set_blob(handle,key,&candidate,sizeof(candidate))" in src
assert "nvs_commit(handle)" in src
assert "prefs.clear()" not in src
assert "nvs_legacy_preserved" in src
assert "curve_storage" in src
assert "SPIFFS.begin(false" in fs
assert "if(!erased(part))" in fs
assert "SPIFFS.begin(true" in fs
assert "SPIFFS.remove(destination)" in fs
assert "SPIFFS.rename(kTemp,destination)" in fs
assert "memcmp(&readback,&candidate,sizeof(candidate))" in fs
assert "curves::curveValid(out)" in fs
assert "ESP_PARTITION_SUBTYPE_DATA_SPIFFS" in fs
assert "0x670000UL" in fs and "0x180000UL" in fs
assert "nvs, data, nvs, 0x9000, 0x5000," in part
assert "spiffs, data, spiffs, 0x670000, 0x180000," in part
assert "pv.minute==1440" in policy and "ch.minute==1440" in policy
print("PASS RECOVERY3.5 filesystem/NVS compatibility and non-destructive guards")
