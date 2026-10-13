#pragma once
// Application services used by UI callbacks/workflows, without ESP reset/sleep APIs.
#include <Arduino.h>
#include "runtime.h"
#include "time_rules.h"
#include "day_rules.h"
#include "save_format.h"
#include "../config/game_config.h"
#include "../config/food_table.h"
#include "../core/time_manager.h"
#include "../core/save_manager.h"
#include "../input/menu_controller.h"
#include "../display/DisplayManager.h"
extern PetState pet;
extern DeviceState deviceState;
extern bool waitingForTimeSet;
extern bool reinitPetAfterInitialTimeConfirm;
extern UICallbacks gameCallbacks;
SaveResult persistDeviceState();
void persistGalleryUnlockFromEvolution(const EvolutionResult&);
void persistCurrentFormUnlock();
void showWhiteRhongoJustEntered(Form,int16_t);
void doDayEnd();
void processElapsedTime();
void restoreLoadedClock();
SaveResult autoSaveCurrentPet();
void doReset();
void skipTime(uint32_t);
void printStatus();
