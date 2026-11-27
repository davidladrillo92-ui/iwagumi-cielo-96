#include "cielo_report315.h"
#include <string>
#include <cassert>
#include <cstdio>
using namespace cielo96::report315;
struct B{std::string s;B(){}B(const char* p):s(p){}B(char c):s(1,c){}bool reserve(size_t n){s.reserve(n);return true;}size_t length()const{return s.size();}const char* c_str()const{return s.c_str();}void remove(size_t){s.clear();}bool concat(const B&b){s+=b.s;return true;}};
struct S{std::string text;unsigned sends=0;bool fail=false;bool send(const B& b){++sends;if(fail)return false;text+=b.s;return true;}};
int main(){assert(validNonce("0123456789abcdef")&&!validNonce("short")&&!validNonce("0123456789\"abc"));S s;Writer<B,S> w(s);assert(w.begin());w+="{";B big;big.s.assign(5000,'x');w+=big;for(int i=0;i<5000;i++)w+='y';w+="}";assert(w.flush());assert(w.bytes()==10002&&w.peak()<=4096&&s.text=="{"+big.s+std::string(5000,'y')+"}");S dead;Writer<B,S> d(dead);assert(d.begin());d+="abc";dead.fail=true;d+=big;assert(!d.good()&&!d.flush()&&d.bytes()==0);S known;Writer<B,S> k(known);assert(k.begin());k+="123456789";assert(k.checksum()==0xcbf43926U);k+="";assert(!k.good());puts("PASS bounded report chunks, nonce, CRC, allocation sentinel and disconnect");}
