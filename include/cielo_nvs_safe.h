#pragma once
// Host-testable decision policy; never modifies flash or firmware curves.
#include <stddef.h>
#include <stdint.h>
namespace cielo96 { namespace nvssafe {
enum class Failure : uint8_t {
 None, InvalidProfile, StatsUnavailable, InsufficientEntries,
 OpenFailed, WriteFailed, CommitFailed, ReadFailed, VerificationFailed
};
inline const char* message(Failure f) {
 switch(f) {
 case Failure::None:return "ninguno";
 case Failure::InvalidProfile:return "Perfil invalido: no se ha guardado";
 case Failure::StatsUnavailable:return "No se puede comprobar el espacio NVS: perfil anterior conservado";
 case Failure::InsufficientEntries:return "Memoria NVS insuficiente para una copia segura: perfil anterior conservado";
 case Failure::OpenFailed:return "No se pudo abrir NVS: perfil anterior conservado";
 case Failure::WriteFailed:return "Error de escritura NVS: perfil anterior conservado";
 case Failure::CommitFailed:return "Error al confirmar NVS: perfil anterior conservado";
 case Failure::ReadFailed:return "Error al releer NVS: perfil anterior conservado";
 case Failure::VerificationFailed:return "Fallo de verificacion NVS: perfil anterior conservado";
 }
 return "Error NVS desconocido: perfil anterior conservado";
}
// Lower bound, not a guarantee: NVS blobs need data entries plus metadata.
// Reject only when even this lower bound is unavailable. The NVS API
// remains authoritative (page fragmentation and GC cannot be predicted here).
inline size_t minimumFreeEntries(size_t blobBytes) {
 return (blobBytes + 31u) / 32u + 2u;
}
inline bool definitelyTooLittleSpace(size_t freeEntries,size_t blobBytes) {
 return freeEntries < minimumFreeEntries(blobBytes);
}
// The only valid slot must never be the write target. If both are valid,
// overwrite the older generation, never the active latest generation.
inline const char* target(bool validA,uint32_t genA,bool validB,uint32_t genB) {
 if(validA && !validB)return "curvesB";
 if(!validA && validB)return "curvesA";
 if(!validA && !validB)return "curvesA";
 return static_cast<int32_t>(genA-genB)>0 ? "curvesB" : "curvesA";
}
inline bool targetPreservesOnlyCopy(bool validA,bool validB,const char* key) {
 if(validA && !validB)return key && key[6]=='B';
 if(!validA && validB)return key && key[6]=='A';
 return true;
}
} }
