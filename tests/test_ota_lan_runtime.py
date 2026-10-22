"""Execute the actual OTA write block with fragmented, oversized and incompatible uploads."""
from pathlib import Path
import subprocess,tempfile
s=Path('src/main.cpp').read_text();start=s.index('  const esp_partition_t* target=',s.index('}else if(upload.status==UPLOAD_FILE_WRITE)'))
block=s[start:s.index('\n }else if(upload.status==UPLOAD_FILE_END)',start)]
fixture=r'''
#include "cielo_ota_validation.h"
#include <cassert>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>
struct Partition{size_t size=4096;} partition;
using esp_partition_t=Partition;
const Partition*esp_ota_get_next_update_partition(void*){return &partition;}
struct {uint8_t*buf;size_t currentSize;} upload;
size_t otaBytes=0;bool otaError=false;unsigned otaLastChunkAt=0;
uint8_t otaPrefix[cielo96::ota::prefixBytes]{};
std::string lastOtaError;int otaDigest=0;
void otaDigestStop(){}
unsigned millis(){return 100;}
int mbedtls_sha256_update_ret(int*,uint8_t*,size_t){return 0;}
struct Updater {
 std::vector<uint8_t> bytes;bool aborted=false;
 size_t write(uint8_t*p,size_t n){bytes.insert(bytes.end(),p,p+n);return n;}
 void abort(){aborted=true;}
} Update;
void writeChunk(){
'''
main=r'''
}
int main(){
 std::vector<uint8_t> image(1400,0x12);
 image[0]=0xe9;image[1]=5;image[3]=0x3f;image[12]=9;image[13]=0;
 image[32]=0x32;image[33]=0x54;image[34]=0xcd;image[35]=0xab;
 for(size_t chunk:{1U,7U,287U,288U,289U,1024U}){
  Update={};otaBytes=0;otaError=false;
  for(size_t pos=0;pos<image.size();){
   size_t n=std::min(chunk,image.size()-pos);upload={image.data()+pos,n};writeChunk();pos+=n;
   assert(!otaError);if(pos<288)assert(Update.bytes.empty());
  }
  assert(Update.bytes==image&&otaBytes==image.size());
 }
 // Non-S3 or non-app image never reaches the flash writer.
 Update={};otaBytes=0;otaError=false;image[12]=0;
 upload={image.data(),image.size()};writeChunk();
 assert(otaError&&Update.aborted&&Update.bytes.empty());
 Update={};otaBytes=0;otaError=false;image[12]=9;partition.size=1000;
 writeChunk();assert(otaError&&Update.aborted&&Update.bytes.empty());
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.cpp';p.write_text(fixture+block+main);out=str(Path(d)/'test')
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-Iinclude',str(p),'-o',out],check=True)
 subprocess.run([out],check=True)
print('PASS actual OTA writer: fragmented prefix, exact bytes, incompatible chip and partition overflow')
