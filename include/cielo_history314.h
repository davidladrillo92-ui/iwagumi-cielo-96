#pragma once
#include <stdint.h>
#include <stddef.h>
namespace cielo96 { namespace history314 {
// Lightweight 30-day aggregation. RAM only; no flash writes, no file mutations.
// day_key uses Europe/Madrid local calendar: tm_year*1000+tm_yday.
struct Day {
 uint32_t day_key=0,samples=0,pv_ok=0,ch_ok=0,pv_errors=0,ch_errors=0,heap_min=0;
};
class Ring {
 Day days_[30]{};
 uint8_t next_=0,count_=0;
public:
 uint8_t count()const{return count_;}
 const Day& oldest(uint8_t offset)const {
  return days_[(uint8_t)((next_+30-count_+offset)%30)];
 }
 void sample(uint32_t day_key,uint32_t pv_inc,uint32_t ch_inc,
             uint32_t pv_err_inc,uint32_t ch_err_inc,uint32_t heap){
  if(day_key<2026000UL || heap==0)return;
  Day* target=nullptr;
  if(count_){
   Day& last=days_[(next_+29)%30];
   if(last.day_key==day_key)target=&last;
   // A backwards clock step must not overwrite a newer day.
   else if(day_key<last.day_key)return;
  }
  if(!target){
   target=&days_[next_];*target=Day{};target->day_key=day_key;
   next_=(next_+1)%30;
   if(count_<30)++count_;
  }
  target->samples++;
  target->pv_ok+=pv_inc;target->ch_ok+=ch_inc;
  target->pv_errors+=pv_err_inc;target->ch_errors+=ch_err_inc;
  if(!target->heap_min || heap<target->heap_min)target->heap_min=heap;
 }
};
}} // cielo96::history314
