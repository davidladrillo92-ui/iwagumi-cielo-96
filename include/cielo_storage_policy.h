#pragma once
#include "cielo_curve_policy.h"
#include <string.h>
namespace cielo96 { namespace storage {
// Compare meaningful points, not generation, checksum or unused array tails.
inline bool sameCurves(const ProfileImage& a,const ProfileImage& b){
 if(!curves::curveValid(a)||!curves::curveValid(b)||a.pvCount!=b.pvCount||a.chCount!=b.chCount)return false;
 for(unsigned i=0;i<a.pvCount;i++){
  const auto& x=a.pv[i];const auto& y=b.pv[i];
  if(x.minute!=y.minute||x.r!=y.r||x.g!=y.g||x.b!=y.b||x.brightness!=y.brightness)return false;
 }
 for(unsigned i=0;i<a.chCount;i++){
  const auto& x=a.ch[i];const auto& y=b.ch[i];
  if(x.minute!=y.minute||x.r!=y.r||x.g!=y.g||x.b!=y.b)return false;
 }
 return true;
}
inline bool canSkipSave(const ProfileImage& active,const ProfileImage& candidate,
                       bool persisted,bool uncertain){
 return persisted&&!uncertain&&sameCurves(active,candidate);
}
}}
