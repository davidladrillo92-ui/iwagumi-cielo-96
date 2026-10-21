#pragma once
#include <WebServer.h>
#include <Preferences.h>
#include "cielo_monitor.h"
namespace cielo96 { namespace monitor {
// Callbacks consultados exclusivamente desde la tarea WebServer principal.
void attach(WebServer& server,Preferences& prefs,bool (*admin)(),bool (*privateRead)(),Snapshot (*snapshot)());
} }
