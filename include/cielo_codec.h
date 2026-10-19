#pragma once
// Tramas Pez Verde validadas por el usuario; Chihiros tomadas de protocolo publico,
// NO confirmadas fisicamente en DYSL60 por este R4.
#include <stdint.h>
#include <stddef.h>
namespace cielo96 { namespace codec {
static const char* const PV_NAME="ELK-BLEDOB";
static const char* const PV_SVC="0000fff0-0000-1000-8000-00805f9b34fb";
static const char* const PV_TX="0000fff3-0000-1000-8000-00805f9b34fb";
static const char* const CH_NAME="DYSL60";
static const char* const CH_SVC="6e400001-b5a3-f393-e0a9-e50e24dcca9e";
static const char* const CH_TX="6e400002-b5a3-f393-e0a9-e50e24dcca9e";
inline void pv_power(bool on,uint8_t out[9]) {
 const uint8_t t[9]={0x7e,0x07,0x04,on?uint8_t(0xff):uint8_t(0),0x00,on?uint8_t(1):uint8_t(0),0x02,0x01,0xef};
 for(int i=0;i<9;i++)out[i]=t[i];
}
inline void pv_color(uint8_t r,uint8_t g,uint8_t b,uint8_t out[9]) {
 const uint8_t t[9]={0x7e,0x07,0x05,0x03,r,g,b,0x10,0xef};for(int i=0;i<9;i++)out[i]=t[i];
}
inline void pv_brightness(uint8_t percent,uint8_t out[9]) {
 const uint8_t t[9]={0x7e,0x04,0x01,uint8_t(percent>100?100:percent),0x01,0xff,0x02,0x01,0xef};
 for(int i=0;i<9;i++)out[i]=t[i];
}
inline uint8_t ch_xor(const uint8_t* p,size_t length_without_crc){uint8_t x=0;for(size_t i=1;i<length_without_crc;i++)x^=p[i];return x;}
// Respeta byte reservado 0x5A (ID, payload y checksum). Un ID invalido se descarta.
inline bool ch_frame(uint8_t family,uint8_t mode,const uint8_t* payload,size_t n,
                     uint16_t id,uint8_t* out,size_t cap,size_t& len){
 len=0;if(n>32||cap<n+7||(!payload&&n))return false;
 if((id>>8)==0x5a||(id&0xff)==0x5a)return false;
 out[0]=family;out[1]=0x01;out[2]=uint8_t(n+5);out[3]=uint8_t(id>>8);out[4]=uint8_t(id);
 out[5]=mode;for(size_t i=0;i<n;i++)out[6+i]=payload[i]==0x5a?0x59:payload[i];
 uint8_t crc=ch_xor(out,n+6);if(crc==0x5a)return false;
 out[n+6]=crc;len=n+7;return true;
}
// Busca un ID valido tambien cuando el XOR coincide con 0x5a.
inline bool ch_safe_frame(uint8_t family,uint8_t mode,const uint8_t* payload,size_t n,
                          uint16_t& id,uint8_t* out,size_t cap,size_t& len){
 for(unsigned i=0;i<600;i++){
  uint16_t tryid=++id;
  if(ch_frame(family,mode,payload,n,tryid,out,cap,len))return true;
 }
 return false;
}
inline bool ch_manual(uint8_t channel,uint8_t percent,uint16_t& id,uint8_t* out,size_t cap,size_t& len){
 if(channel>3||percent>100)return false;
 const uint8_t p[]={channel,percent};return ch_safe_frame(0x5a,0x07,p,2,id,out,cap,len);
}
inline bool ch_auto(uint16_t& id,uint8_t* out,size_t cap,size_t& len){
 const uint8_t p[]={18,255,255};return ch_safe_frame(0x5a,0x05,p,3,id,out,cap,len);
}
inline bool ch_manual_prelude(uint16_t& id,uint8_t* out,size_t cap,size_t& len){
 const uint8_t p[]={11,255,255};return ch_safe_frame(0x5a,0x05,p,3,id,out,cap,len);
}
} }
