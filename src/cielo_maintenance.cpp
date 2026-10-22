#include "cielo_maintenance.h"
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <esp_heap_caps.h>
#include <nvs.h>
namespace cielo96 {
String maintenanceInventory(){
 String j;j.reserve(2800);
 j="{\"schema\":\"cielo-maintenance-v1\",\"flash_bytes\":"+String(ESP.getFlashChipSize());
 j+=",\"heap_free\":"+String(heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
 j+=",\"heap_min\":"+String(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
 j+=",\"largest_block\":"+String(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
 nvs_stats_t stats{};const bool nvsOk=nvs_get_stats(nullptr,&stats)==ESP_OK;
 j+=",\"nvs_stats_available\":"+String(nvsOk?"true":"false");
 if(nvsOk)j+=",\"nvs_used_entries\":"+String(stats.used_entries)+",\"nvs_free_entries\":"+String(stats.free_entries);
 j+=",\"filesystem\":\"NOT_MOUNTED_NO_FORMAT\",\"files\":null,\"partitions\":[";
 const esp_partition_t* running=esp_ota_get_running_partition();
 esp_partition_iterator_t it=esp_partition_find(ESP_PARTITION_TYPE_ANY,ESP_PARTITION_SUBTYPE_ANY,nullptr);
 bool first=true;
 while(it){
  const esp_partition_t*p=esp_partition_get(it);
  if(!first)j+=",";first=false;
  // Labels provienen de tabla local, se restringen a ASCII seguro.
  char label[17]{};for(size_t i=0;i<16&&p->label[i];i++){
   const char c=p->label[i];label[i]=((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_')?c:'_';
  }
  j+="{\"label\":\""+String(label)+"\",\"type\":"+String(p->type)+",\"subtype\":"+String(p->subtype);
  j+=",\"offset\":"+String(p->address)+",\"size\":"+String(p->size)+",\"encrypted\":"+String(p->encrypted?"true":"false");
  j+=",\"running\":"+String(running&&running->address==p->address?"true":"false")+"}";
  it=esp_partition_next(it); // IDF libera el iterador al llegar al final.
 }
 j+="],\"flash_backup\":\"USB_REQUIRED_NOT_PERFORMED\",\"optical_feedback\":null}";
 return j;
}
}
