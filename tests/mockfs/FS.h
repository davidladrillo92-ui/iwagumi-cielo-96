#pragma once
#include <map>
#include <string>
#include <vector>
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <algorithm>
struct MockSPIFFS;
struct File {
 MockSPIFFS* fs=nullptr;
 std::string path;
 size_t offset=0;
 bool readable=false,writeable=false;
 File()=default;
 File(MockSPIFFS* f,const char* p,bool r,bool w):fs(f),path(p),readable(r),writeable(w){}
 explicit operator bool() const {return fs!=nullptr;}
 size_t size() const;
 size_t read(uint8_t* data,size_t len);
 size_t write(const uint8_t* data,size_t len);
 void flush(){}
 void close(){}
};
struct MockSPIFFS {
 bool formatted=false;
 bool fail_write=false, fail_rename=false;
 bool fail_read_once=false;std::string fail_read_path;
 size_t read_calls=0,fail_read_at=0;
 bool fail_mount=false;
 int format_count=0;
 std::map<std::string,std::vector<uint8_t>> files;
 bool begin(bool formatOnFail,const char*,uint8_t,const char*){
  if(fail_mount)return false;
  if(formatted)return true;
  if(formatOnFail){formatted=true;++format_count;return true;}
  return false;
 }
 bool exists(const char* p) const {return files.count(p)>0;}
 bool remove(const char* p){return files.erase(p)>0;}
 bool rename(const char* a,const char* b){
  if(fail_rename || !exists(a))return false;
  files[b]=files[a];files.erase(a);return true;
 }
 File open(const char* p,const char* mode){
  if(!formatted)return File{};
  const bool wr=mode && mode[0]=='w';
  if(wr){files[p].clear();return File(this,p,false,true);}
  if(!exists(p))return File{};
  return File(this,p,true,false);
 }
 size_t totalBytes() const{return 0x180000;}
 size_t usedBytes() const{
  size_t n=0;for(const auto& v:files)n+=v.second.size();return n;
 }
} ;
inline MockSPIFFS SPIFFS;
inline size_t File::size() const {return fs?fs->files.at(path).size():0;}
inline size_t File::read(uint8_t* data,size_t n){
 if(!fs||!readable)return 0;
 ++fs->read_calls;
 if(fs->fail_read_at==fs->read_calls)return 0;
 if(fs->fail_read_once&&path==fs->fail_read_path){fs->fail_read_once=false;return 0;}
 const auto& v=fs->files.at(path);
 const size_t cnt=std::min(n,v.size()-std::min(offset,v.size()));
 if(cnt)memcpy(data,v.data()+offset,cnt);
 offset+=cnt;return cnt;
}
inline size_t File::write(const uint8_t* data,size_t n){
 if(!fs||!writeable)return 0;
 if(fs->fail_write)n=std::min(n,size_t(16));
 auto& v=fs->files.at(path);
 v.insert(v.end(),data,data+n);
 return n;
}
