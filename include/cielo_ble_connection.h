#pragma once
#include <stdint.h>
namespace cielo96 { namespace connection {
constexpr uint32_t CONNECT_MS=8000, SCAN_MS=5000;
// The owning worker waits cooperatively; callbacks only publish link state.
// cancel() must be the BLE library's cancellation API, never task deletion.
template<class Clock,class State,class Cancel,class Yield>
bool await(uint32_t start,Clock clock,State state,Cancel cancel,Yield yield){
 for(;;){
  const int status=state();
  if(status>0)return true;
  if(status<0)break;
  if(uint32_t(clock()-start)>=CONNECT_MS)break;
  yield();
 }
 cancel();return false;
}
} }
