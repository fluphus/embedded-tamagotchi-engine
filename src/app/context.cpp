#include "context.h"
PetState pet;
DeviceState deviceState;
char cmdBuf[64]{};
uint8_t cmdLen=0;
bool waitingForTimeSet=false;
bool reinitPetAfterInitialTimeConfirm=false;
SlotImportSession g_slotImport;
RTC_DATA_ATTR uint64_t rtc_us_at_sleep=0;
RTC_DATA_ATTR uint32_t epoch_at_sleep=0;
