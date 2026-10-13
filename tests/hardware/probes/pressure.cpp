// Test firmware only: hold external RAM before the real display initializes.
#include <Arduino.h>
#include <esp_heap_caps.h>
static void* reservations[96];
extern "C" void initVariant(){
    size_t n=0;
    for(;n<64;++n){reservations[n]=heap_caps_malloc(128*1024,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!reservations[n])break;}
    for(;n<96;++n){reservations[n]=heap_caps_malloc(4096,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!reservations[n])break;}
}
