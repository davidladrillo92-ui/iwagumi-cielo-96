#include "cielo_littlefs_readonly.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
using cielo96::storage::LittlefsReadonlyProbe;
void reset(){
 std::fill(mock_flash.begin(),mock_flash.end(),0xff);mock_partition={0x670000,0x180000,false};mock_partition_available=true;
 mock_read_calls=mock_fail_read_at=0;lfs_register_calls=lfs_unregister_calls=lfs_info_calls=0;lfs_mounted=false;
 lfs_register_error=lfs_info_error=lfs_unregister_error=ESP_OK;lfs_total=0x180000;lfs_used=72*4096;
 memcpy(mock_flash.data()+8,"littlefs",8);memcpy(mock_flash.data()+4104,"littlefs",8);
}
int main(){
 reset();const auto before=mock_flash;LittlefsReadonlyProbe p;p.run();
 assert(p.verified&&!lfs_mounted&&!p.retainedReadonlyMount&&p.totalBytes==0x180000&&p.usedBytes==72*4096);
 assert(lfs_register_calls==1&&lfs_unregister_calls==1&&mock_flash==before);
 p.run();assert(lfs_register_calls==1); // cached; polling never remounts
 reset();mock_partition.address=0;LittlefsReadonlyProbe wrong;wrong.run();assert(!wrong.mountAttempted&&mock_read_calls==0);
 reset();mock_partition.encrypted=true;LittlefsReadonlyProbe enc;enc.run();assert(!enc.mountAttempted&&mock_read_calls==0);
 reset();mock_fail_read_at=2;LittlefsReadonlyProbe io;io.run();assert(!io.mountAttempted&&io.readError==ESP_FAIL);
 reset();std::fill(mock_flash.begin(),mock_flash.end(),0xff);LittlefsReadonlyProbe blank;blank.run();assert(!blank.mountAttempted);
 reset();lfs_mounted=true;LittlefsReadonlyProbe owned;owned.run();assert(!owned.mountAttempted&&lfs_unregister_calls==0&&lfs_mounted);
 reset();lfs_register_error=ESP_FAIL;LittlefsReadonlyProbe bad;bad.run();assert(!bad.verified&&lfs_unregister_calls==0&&bad.mountError==ESP_FAIL);
 reset();lfs_info_error=ESP_FAIL;LittlefsReadonlyProbe info;info.run();assert(!info.verified&&lfs_unregister_calls==1&&!lfs_mounted);
 reset();lfs_unregister_error=ESP_FAIL;LittlefsReadonlyProbe unmount;unmount.run();assert(!unmount.verified&&unmount.retainedReadonlyMount);
 reset();lfs_used=lfs_total+1;LittlefsReadonlyProbe size;size.run();assert(!size.verified&&!lfs_mounted);
 reset();std::fill(mock_flash.begin(),mock_flash.begin()+16,0xff);LittlefsReadonlyProbe mirror;mirror.run();assert(mirror.verified&&!mirror.signature0&&mirror.signature1);
 std::cout<<"PASS LittleFS probe: read-only/no-format/no-grow, known geometry/signatures, errors, cleanup, ownership, mirror and one-shot cache\n";
}
