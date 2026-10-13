#pragma once
#include "../content/definition.h"
#include <Arduino.h>
#include "save_protocol.h"
#include "runtime.h"
#include "time_rules.h"
#include "serial_codec.h"
#include "day_rules.h"
#include "save_format.h"
#include "../config/game_config.h"
#include "../config/food_table.h"
#include "../core/game_state.h"
#include "../core/time_manager.h"
#include "../core/save_manager.h"
#include "../pet/feeding.h"
#include "../pet/seriousness.h"
#include "../pet/evolution.h"
#include "../pet/gallery.h"
#include "../input/menu_controller.h"
#include "../display/DisplayManager.h"
#include "../core/power_manager.h"
#include <esp_sleep.h>
#include <esp_random.h>
#include <esp_system.h>
#include <esp32/rtc.h>
#include <driver/rtc_io.h>
#include <string.h>

// ============================================================================
//  Serial input debug switch (set to 0 to disable serial button commands)
// ============================================================================
#ifndef ENABLE_SERIAL_INPUT_DEBUG
#define ENABLE_SERIAL_INPUT_DEBUG 1
#endif


extern PetState pet;
extern DeviceState deviceState;
extern char cmdBuf[64];
extern uint8_t cmdLen;
extern bool waitingForTimeSet;
extern bool reinitPetAfterInitialTimeConfirm;
inline constexpr size_t SLOT_RAW_BASE_BYTES=engine::PetBytes;
inline constexpr size_t SLOT_RAW_WITH_GALLERY_BYTES=engine::FullBytes;
inline constexpr size_t SLOT_RAW_MAX_BYTES=engine::MaxSaveBytes;
inline constexpr size_t EXPORT_LINE_BYTES=16;
inline constexpr uint8_t SAVE_IMPORT_INVALID_MAX=5;
using SlotImportSession=app::SlotImportSession;
extern SlotImportSession g_slotImport;
extern uint64_t rtc_us_at_sleep;
extern uint32_t epoch_at_sleep;
extern UICallbacks gameCallbacks;
void realignOwnerTimestamps(PetState& ownerPet, uint32_t frozenDuration);
void realignImportedTimestamps(PetState& pet, uint32_t donorSaveTime, uint32_t localNow, uint8_t localDay);
SaveResult persistDeviceState();
void persistGalleryUnlockFromEvolution(const EvolutionResult& r);
void persistCurrentFormUnlock();
void showWhiteRhongoJustEntered(Form formBefore, int16_t srAfter);
void doDayEnd();
void processElapsedTime();
void restoreLoadedClock();
SaveResult autoSaveCurrentPet();
void doReset();
void skipTime(uint32_t offlineSeconds);
int hexNibble(char c);
bool decodeHexToBytes(const char* hex, uint8_t* out, size_t maxOut, size_t* outLen);
void printHexLine(const uint8_t* data, size_t len);
bool parseSlotArg(const char* arg, uint8_t* slotOut);
void printStatus();
void printStatusForSnapshot(const PetState& p, uint32_t epochNow);
void printHelp();
void processCommand(const char* cmd);
void readSerialCommand();
const char* wakeupCauseName(esp_sleep_wakeup_cause_t c);
const char* resetReasonName(esp_reset_reason_t r);
void enterDeepSleep();
