#pragma once
#include "cielo_curve_policy.h"
#include "cielo_nvs_safe.h"
#include <string.h>
namespace cielo96 { namespace profiletxn {
struct Result {
 nvssafe::Failure failure=nvssafe::Failure::None;
 const char* stage="sin_intento";
 int error=0,initialWriteError=0;
 bool recoveryAttempted=false,inactiveReclaimed=false,writeAccepted=false,verified=false;
 bool writeMayHavePersisted=false;
};
// Store methods use fresh NVS handles. The caller owns serialization and RAM
// activation. Only the inactive curve key can be removed, never other data.
template<class Store>
Result save(Store& store,const char* target,const char* survivor,
            const ProfileImage& active,const ProfileImage& candidate,bool bothValid){
 Result r;
 auto fail=[&](nvssafe::Failure why,const char* stage,int error){r.failure=why;r.stage=stage;r.error=error;return r;};
 if(!curves::curveValid(candidate)||candidate.generation!=uint32_t(active.generation+1))return fail(nvssafe::Failure::InvalidProfile,"validacion",Store::Unsafe);
 if(!target || (strcmp(target,"curvesA")!=0&&strcmp(target,"curvesB")!=0) ||
    (survivor && ((strcmp(survivor,"curvesA")!=0&&strcmp(survivor,"curvesB")!=0)||strcmp(target,survivor)==0)))
  return fail(nvssafe::Failure::ActiveMismatch,"proteger_activo",Store::Unsafe);
 static ProfileImage check{}; // HTTP/loop is the single caller, no BLE callbacks.
 auto protectedCopyValid=[&](){
  check=ProfileImage{};
  return survivor && store.read(survivor,check)==Store::Ok &&
   curves::curveValid(check) && memcmp(&check,&active,sizeof(check))==0;
 };
 if(survivor && !protectedCopyValid())return fail(nvssafe::Failure::ActiveMismatch,"proteger_activo",Store::Unsafe);
 int err=store.write(target,candidate);r.initialWriteError=err;
 // The SDK can publish a new blob, then fail while retiring its previous
 // version (e.g. ESP_ERR_NVS_REMOVE_FAILED). A non-NoSpace error is uncertain.
 r.writeMayHavePersisted=err!=Store::NoSpace;
 if(err==Store::NoSpace && bothValid && survivor){
  r.recoveryAttempted=true;
  // Re-read immediately before changing the redundant slot. Failure closes
  // this path without erasing anything. NVS retains the old blob during set.
  if(!protectedCopyValid())return fail(nvssafe::Failure::ActiveMismatch,"releer_activo",Store::Unsafe);
  err=store.erase(target);
  if(err!=Store::Ok)return fail(nvssafe::Failure::ReclaimFailed,"liberar_inactivo",err);
  r.inactiveReclaimed=true;
  if((err=store.commit())!=Store::Ok)return fail(nvssafe::Failure::CommitFailed,"confirmar_liberacion",err);
  if(!protectedCopyValid())return fail(nvssafe::Failure::ActiveMismatch,"verificar_activo",Store::Unsafe);
  err=store.write(target,candidate); // exactly one recovery retry
  r.writeMayHavePersisted=err!=Store::NoSpace;
 }
 if(err!=Store::Ok)return fail(nvssafe::Failure::WriteFailed,"escribir",err);
 r.writeAccepted=true;
 if((err=store.commit())!=Store::Ok)return fail(nvssafe::Failure::CommitFailed,"confirmar",err);
 check=ProfileImage{};
 if((err=store.read(target,check))!=Store::Ok)return fail(nvssafe::Failure::ReadFailed,"releer",err);
 if(!curves::curveValid(check)||memcmp(&candidate,&check,sizeof(check))!=0)
  return fail(nvssafe::Failure::VerificationFailed,"verificar",Store::Corrupt);
 r.verified=true;r.stage="correcto";return r;
}
} }
