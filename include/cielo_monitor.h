#pragma once
#include <stddef.h>
#include <stdint.h>
#include "cielo_restart_journal.h"
namespace cielo96 { namespace monitor {
constexpr size_t JSON_CAPACITY=6144;
struct Lamp {
 bool expected=false,everOk=false;
 uint32_t okAgeSeconds=0,consecutiveErrors=0,operations=0,errors=0;
};
struct Snapshot {
 const char* firmware="";
 uint32_t bootId=0,uptime=0,epoch=0,heap=0,heapMin=0,largestBlock=0;
 bool chReturnFailed=false;
 bool clockValid=false,bleReady=false,workerStalled=false,ota=false,offPending=false;
 uint32_t offFailures=0,queueDepth=0,workerQueueDepth=0,stackFreeBytes=0;
 uint32_t profileGeneration=0,pvPoints=0,chPoints=0;
 Lamp pv,ch;
 bool bootMarkedHealthy=false,recoveredBoot=false;
 uint8_t resetReason=0,recentCount=0;
 uint32_t restartTotal=0,lastStallPhase=0,lastStallDevice=255,lastStallMs=0;
 restarts::Entry recent[restarts::CAPACITY]{};
};
// Esquema cerrado: no SSID, nombres BLE, IP, tokens, texto libre ni RGB.
// Capacidad fija y comprobada; cero asignaciones y ninguna orden BLE/NVS.
size_t encode(const Snapshot& s,char* out,size_t capacity);
} }
