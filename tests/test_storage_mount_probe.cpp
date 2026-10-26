#include "cielo_profile_fs.h"
#include <cassert>
#include <algorithm>
#include <cstring>
#include <iostream>
using namespace cielo96;
void reset(){SPIFFS=MockSPIFFS{};std::fill(mock_flash.begin(),mock_flash.end(),0xff);mock_read_calls=mock_fail_read_at=0;mock_partition.encrypted=false;mock_partition.address=0x670000;}
int main(){
 reset();profilefs::Store blank;assert(!blank.begin());
 assert(strcmp(blank.rawState(),"ALL_FF")==0&&blank.rawBytesRead()==0x180000);
 assert(!blank.initializedFromBlank()&&SPIFFS.format_count==0);
 reset();mock_fail_read_at=1;profilefs::Store io;
 assert(!io.begin()&&strcmp(io.status(),"PARTITION_READ_FAILED_PRESERVED")==0);
 assert(io.rawError()==ESP_FAIL&&!io.hasFirstData()&&io.rawBytesRead()==0);
 reset();mock_fail_read_at=3;profilefs::Store late;
 assert(!late.begin()&&late.rawBytesRead()==2048&&strcmp(late.rawState(),"READ_ERROR")==0);
 reset();mock_flash.back()=0x12;profilefs::Store nonblank;
 assert(!nonblank.begin()&&nonblank.hasFirstData()&&nonblank.firstDataOffset()==0x17ffff);
 assert(strcmp(nonblank.status(),"UNMOUNTABLE_NONEMPTY_PRESERVED")==0&&nonblank.rawError()==ESP_OK);
 reset();mock_partition.encrypted=true;profilefs::Store encrypted;
 assert(!encrypted.begin()&&encrypted.partitionEncrypted()&&mock_read_calls==0);
 assert(strcmp(encrypted.status(),"ENCRYPTED_PARTITION_UNSUPPORTED_PRESERVED")==0);
 reset();SPIFFS.formatted=true;profilefs::Store mounted;
 assert(mounted.begin()&&mounted.ready()&&strcmp(mounted.rawState(),"NOT_READ")==0&&mock_read_calls==0);
 assert(SPIFFS.format_count==0&&mounted.partitionAddress()==0x670000&&mounted.partitionSize()==0x180000);
 std::cout<<"PASS storage probe: blank/read-error/nonempty/encrypted/mounted distinguished; zero formats and bounded raw reads\n";
}
