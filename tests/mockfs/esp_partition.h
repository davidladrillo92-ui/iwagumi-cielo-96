#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include <cstring>
using esp_err_t=int;
constexpr esp_err_t ESP_OK=0;
constexpr esp_err_t ESP_FAIL=-1;
constexpr int ESP_PARTITION_TYPE_DATA=1;
constexpr int ESP_PARTITION_SUBTYPE_DATA_SPIFFS=130;
struct esp_partition_t {size_t address; size_t size;};
inline std::vector<uint8_t> mock_flash(0x180000,0xff);
inline esp_partition_t mock_partition{0x670000,0x180000};
inline bool mock_partition_available=true;
inline const esp_partition_t* esp_partition_find_first(int,int,const char* label){
 return mock_partition_available && label && strcmp(label,"spiffs")==0?&mock_partition:nullptr;
}
inline esp_err_t esp_partition_read(const esp_partition_t*,size_t offset,void* out,size_t count){
 if(offset+count>mock_flash.size())return ESP_FAIL;
 memcpy(out,mock_flash.data()+offset,count);return ESP_OK;
}
