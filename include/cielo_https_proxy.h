#pragma once
#include <Arduino.h>
#include <Preferences.h>
namespace cielo96 {
// Termina TLS en el propio ATOMS3 y envia solo al WebServer en 127.0.0.1.
// Clave privada TLS por dispositivo, generada y almacenada en NVS; NO GitHub.
bool https_begin(Preferences&prefs);
bool https_ready();
String https_cert_fingerprint();
}
