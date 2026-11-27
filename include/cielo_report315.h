#pragma once
#include <stdint.h>
#include <stddef.h>
namespace cielo96 { namespace report315 {
inline bool validNonce(const char* s){if(!s)return false;size_t n=0;for(;s[n];++n){const char c=s[n];if(n>=64||!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'))return false;}return n>=12;}
// Buffer abstraction keeps the Arduino String and WebServer behind adapters.
// Large helper sections bypass the chunk buffer; failed append never emits a completion marker.
template<class Buffer,class Sink> class Writer {
 Buffer buffer_;Sink& sink_;bool ok_=true;size_t bytes_=0,peak_=0;uint32_t crc_=~0U;
 static constexpr size_t limit=4096;
 void digest(const Buffer& v){const char* p=v.c_str();for(size_t i=0;i<v.length();++i){crc_^=uint8_t(p[i]);for(unsigned b=0;b<8;++b)crc_=(crc_>>1)^(0xedb88320U&(0U-(crc_&1U)));}}
public:
 explicit Writer(Sink& s):sink_(s){}
 bool begin(){return ok_=buffer_.reserve(limit);}
 bool good()const{return ok_;}size_t bytes()const{return bytes_;}size_t peak()const{return peak_;}uint32_t checksum()const{return ~crc_;}
 bool flush(){if(!ok_)return false;if(buffer_.length()){ok_=sink_.send(buffer_);if(ok_)bytes_+=buffer_.length();buffer_.remove(0);}return ok_;}
 Writer& operator+=(const Buffer& v){if(!ok_)return *this;const size_t n=v.length();if(!n){ok_=false;return *this;}digest(v);
  if(n>limit){if(flush()){ok_=sink_.send(v);if(ok_)bytes_+=n;}return *this;}
  if(buffer_.length()+n>limit&&!flush())return *this;
  ok_=buffer_.concat(v);if(buffer_.length()>peak_)peak_=buffer_.length();return *this;
 }
 Writer& operator+=(const char* v){return *this+=Buffer(v);}
 Writer& operator+=(char v){return *this+=Buffer(v);}
};
}}
