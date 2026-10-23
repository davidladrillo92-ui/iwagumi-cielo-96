#pragma once
// CIELO R2 - Planificador de operaciones BLE, SIN transporte ni tramas reales.
// C++17 sin heap, sin Arduino, sin radio, sin red. No apto para flashear el acuario.
#include <stdint.h>
#include <stddef.h>

namespace cielo {
namespace ble {
enum class Device : uint8_t { PezVerde=0, Chihiros=1 };
enum class Action : uint8_t { Apply=0, RestoreAuto=1 };
struct Command {
  Device device=Device::PezVerde;
  Action action=Action::Apply;
  uint8_t r=0,g=0,b=0,brightness=0;
  uint32_t sequence=0; // asignado por Scheduler al crear orden
};
struct Metrics {
  uint32_t submitted=0, superseded=0, expired=0, started=0;
  uint32_t acknowledged=0, failed=0, retries=0, exhausted=0;
  uint32_t soft_timeouts=0, rejected_restore_override=0, stale_acks=0;
};
// millis() es modulo 2^32. Comparaciones válidas para intervalos <2^31 ms.
inline bool elapsed(uint32_t now,uint32_t before,uint32_t duration) {
  return uint32_t(now-before)>=duration;
}

class Scheduler {
public:
  explicit Scheduler(uint32_t soft_timeout_ms=3500, uint32_t min_gap_ms=150,
                     uint32_t retry_base_ms=2000)
     : soft_timeout_ms_(soft_timeout_ms), min_gap_ms_(min_gap_ms),
       retry_base_ms_(retry_base_ms) {}
  // TTL sin signo: 0 rechaza inmediatamente; limitar a menos de 1 dia.
  // RestoreAuto queda pendiente y protegido frente a una orden Apply que la anule.
  bool submit(Device dev, Action act, uint8_t r, uint8_t g, uint8_t b,
              uint8_t brightness, uint32_t now, uint32_t ttl_ms=90000) {
    const int idx=index(dev);
    if(idx<0 || ttl_ms==0 || ttl_ms>86400000UL || brightness>100) return false;
    if (act==Action::Apply &&
        ((queue_[idx].present && queue_[idx].cmd.action==Action::RestoreAuto) ||
         (inflight_ && active_.cmd.device==dev && active_.cmd.action==Action::RestoreAuto))) {
      metrics_.rejected_restore_override++;
      return false;
    }
    Entry e;
    e.cmd.device=dev; e.cmd.action=act; e.cmd.r=r; e.cmd.g=g; e.cmd.b=b;
    e.cmd.brightness=brightness; e.cmd.sequence=next_sequence_++;
    if(next_sequence_==0) next_sequence_=1;
    e.at_ms=now; e.ttl_ms=ttl_ms; e.attempt=0;
    if(queue_[idx].present) metrics_.superseded++;
    queue_[idx]=e; queue_[idx].present=true;
    metrics_.submitted++;
    // No recortar cooldown por cambios ordinarios, para evitar ciclos de fallos.
    return true;
  }
  void tick(uint32_t now) {
    for (int i=0;i<2;i++) {
      if(queue_[i].present && queue_[i].cmd.action!=Action::RestoreAuto &&
         elapsed(now,queue_[i].at_ms,queue_[i].ttl_ms)) {
        queue_[i].present=false; metrics_.expired++;
      }
    }
    // Un timeout blando NO libera el bus: la operación BLE real puede seguir dentro
    // del controlador. Solo la confirmación de la tarea propietaria permite continuar.
    if(inflight_ && !soft_timeout_reported_ &&
       elapsed(now,started_ms_,soft_timeout_ms_)) {
      soft_timeout_reported_=true;
      metrics_.soft_timeouts++;
    }
  }
  bool start_next(uint32_t now,Command& cmd,bool prioritize_pv_off=false) {
    tick(now);
    if(inflight_) return false;
    int pick=-1;
    // Explicit PV OFF may precede ordinary RGB. Cooldowns still bound retries;
    // while PV is cooling down the other lamp can continue.
    if(prioritize_pv_off && ready(0,now) && queue_[0].cmd.action==Action::Apply &&
       queue_[0].cmd.brightness==0)pick=0;
    // Restaurar AUTO antes que cualquier cambio visual: prioridad de seguridad.
    for(int i=0;i<2;i++) {
      if(pick<0 && ready(i,now) && queue_[i].cmd.action==Action::RestoreAuto) { pick=i; break; }
    }
    if(pick<0) {
      if(ready(next_device_,now)) pick=next_device_;
      else if(ready(1-next_device_,now)) pick=1-next_device_;
    }
    if(pick<0) return false;
    active_=queue_[pick];
    queue_[pick].present=false;
    cmd=active_.cmd;
    inflight_=true;
    started_ms_=now;
    soft_timeout_reported_=false;
    metrics_.started++;
    next_device_=1-pick;
    return true;
  }
  // success significa que el transporte devolvió finalización positiva;
  // NO implica confirmación óptica de que la pantalla cambió.
  bool complete(uint32_t sequence, bool success, uint32_t now) {
    if(!inflight_ || sequence!=active_.cmd.sequence) {metrics_.stale_acks++;return false;}
    const int idx=index(active_.cmd.device);
    inflight_=false;
    soft_timeout_reported_=false;
    next_allowed_[idx]=now;
    gap_started_[idx]=true;
    if(success) { metrics_.acknowledged++; failures_[idx]=0; return true; }
    metrics_.failed++;
    if(failures_[idx]<8) ++failures_[idx];
    uint32_t shift=(failures_[idx]>5?5:(failures_[idx]-1));
    uint32_t retry_delay=retry_base_ms_<<shift;
    if(retry_delay>60000UL) retry_delay=60000UL;
    cooldown_started_[idx]=now;
    cooldown_ms_[idx]=retry_delay;
    if(active_.attempt>=2 || (active_.cmd.action!=Action::RestoreAuto &&
                              elapsed(now,active_.at_ms,active_.ttl_ms))) {
      metrics_.exhausted++;
      return true;
    }
    if(!queue_[idx].present) {
      active_.attempt++;
      queue_[idx]=active_;
      queue_[idx].present=true;
      metrics_.retries++;
    }
    // Si apareció un estado más reciente, jamás reproducir el antiguo.
    return true;
  }
  // Al ceder una lampara, descartar RGB en espera para no reactivarlos
  // despues del retorno a la curva almacenada. El in-flight se deja acabar.
  bool cancel_pending(Device d) {
    const int i=index(d);if(i<0)return false;
    const bool existed=queue_[i].present;
    queue_[i].present=false;
    return existed;
  }
  bool busy() const {return inflight_;}
  bool pending(Device d) const {const int i=index(d);return i>=0 && queue_[i].present;}
  uint32_t active_sequence() const {return inflight_?active_.cmd.sequence:0;}
  bool stalled() const {return inflight_ && soft_timeout_reported_;}
  const Metrics& metrics() const {return metrics_;}
private:
  struct Entry {Command cmd{};uint32_t at_ms=0,ttl_ms=0;uint8_t attempt=0;bool present=false;};
  static int index(Device d) {int v=static_cast<int>(d);return (v==0||v==1)?v:-1;}
  bool ready(int i,uint32_t now) const {
    if(!queue_[i].present) return false;
    if(gap_started_[i] && !elapsed(now,next_allowed_[i],min_gap_ms_)) return false;
    if(failures_[i] && !elapsed(now,cooldown_started_[i],cooldown_ms_[i])) return false;
    return true;
  }
  Entry queue_[2]{};
  Entry active_{};
  bool inflight_=false;
  bool soft_timeout_reported_=false;
  uint32_t started_ms_=0;
  uint32_t next_sequence_=1;
  uint32_t next_allowed_[2]={0,0};
  bool gap_started_[2]={false,false};
  uint32_t cooldown_started_[2]={0,0};
  uint32_t cooldown_ms_[2]={0,0};
  uint8_t failures_[2]={0,0};
  uint8_t next_device_=0;
  uint32_t soft_timeout_ms_,min_gap_ms_,retry_base_ms_;
  Metrics metrics_{};
};
} // namespace ble
} // namespace cielo
