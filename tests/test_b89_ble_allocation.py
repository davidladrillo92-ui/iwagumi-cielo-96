"""Compila la función REAL ble_begin con FreeRTOS simulado y falla cada reserva."""
from pathlib import Path
import subprocess
import tempfile
code=Path('src/cielo_ble_worker.cpp').read_text()
function=code[code.index('bool ble_begin(){'):code.index('uint32_t ble_stack_free_bytes()')]
prefix=r'''
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <set>
using UBaseType_t=unsigned;using QueueHandle_t=void*;using TaskHandle_t=void*;
namespace cielo {namespace ble {struct Command{char a[32];};}}
struct BleResult{char a[32];};struct BleScanResult{char a[512];};struct BleProbeResult{char a[64];};struct BleCustomResumeResult{char a[64];};
QueueHandle_t requests=nullptr,responses=nullptr,scanRequests=nullptr,scanResponses=nullptr,probeRequests=nullptr,probeResponses=nullptr,releaseRequests=nullptr,customRequests=nullptr,customResults=nullptr;
TaskHandle_t workerHandle=nullptr;int calls=0,failAt=-1;bool taskFail=false;std::set<void*> live;
void task(void*){}constexpr int pdPASS=1;
void* xQueueCreate(unsigned,unsigned){int n=calls++;if(n==failAt)return nullptr;void*p=new char[1];live.insert(p);return p;}
void vQueueDelete(void*p){assert(live.erase(p)==1);delete[] static_cast<char*>(p);}
int xTaskCreatePinnedToCore(void(*)(void*),const char*,unsigned,void*,int,void**h,int){if(taskFail)return 0;*h=reinterpret_cast<void*>(0x1);return 1;}
'''
suffix=r'''
int main(){for(int i=0;i<9;i++){calls=0;failAt=i;assert(!ble_begin());assert(live.empty());assert(!requests&&!responses&&!scanRequests&&!scanResponses&&!probeRequests&&!probeResponses&&!releaseRequests&&!customRequests&&!customResults);}
failAt=-1;taskFail=true;calls=0;assert(!ble_begin());assert(live.empty());assert(!workerHandle);
taskFail=false;calls=0;assert(ble_begin());assert(live.size()==9);assert(calls==9);assert(ble_begin());assert(calls==9);
for(auto p:live)delete[] static_cast<char*>(p);}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.cpp').write_text(prefix+function+suffix)
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS B9: 9 fallos de colas, fallo tarea, reintento limpio y un solo worker')
