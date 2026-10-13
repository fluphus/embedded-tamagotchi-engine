#include "import_workflow.h"
#include "context.h"
#include "../presentation/animation_queue.h"

void printHexLine(const uint8_t* data, size_t len) {
    static const char HEX_MAP[] = "0123456789ABCDEF";
    char line[EXPORT_LINE_BYTES * 2 + 1];
    if (len > EXPORT_LINE_BYTES) len = EXPORT_LINE_BYTES;
    for (size_t i = 0; i < len; i++) {
        line[i * 2] = HEX_MAP[(data[i] >> 4) & 0x0F];
        line[i * 2 + 1] = HEX_MAP[data[i] & 0x0F];
    }
    line[len * 2] = '\0';
    Serial.printf("SAVE_EXPORT_DATA %s\n", line);
}

bool parseSlotArg(const char* arg, uint8_t* slotOut) {
    while (*arg == ' ') arg++;
    char* end = nullptr;
    long v = strtol(arg, &end, 10);
    if (end == arg || *end != '\0' || v < 0 || v >= (long)SAVE_SLOT_COUNT) return false;
    *slotOut = (uint8_t)v;
    return true;
}

// ============================================================================
//  UICallbacks implementation (updates DisplayManager)
// ============================================================================

void printStatus() {
    char timeBuf[24];
    timeManager.getFormattedFull(timeBuf, sizeof(timeBuf));
    uint32_t now = timeManager.now();

    Serial.println("========================================");
    if (pet.is_nobu) {
        Serial.printf("  Time:       %s\n", timeBuf);
        Serial.println("  Name:       nobu");
        Serial.println("  HP:         ?");
        Serial.println("  SR:         ?");
        Serial.println("  Age:        ?");
        Serial.println("  Stage:      ?");
        Serial.println("  Align:      ?");
        Serial.println("========================================");
        return;
    }
    Serial.printf("  Time:       %s\n", timeBuf);
    Serial.printf("  Form:       %s\n", content::formName(pet.form));
    Serial.printf("  Base:       %s\n", content::formName(pet.base_form));
    Serial.printf("  Stage:      %s\n", content::StageNames[pet.stage]);
    Serial.printf("  Alignment:  %s\n", ALIGNMENT_NAMES[pet.alignment]);
    Serial.printf("  Health:     %d / %d\n", pet.health, HEALTH_MAX);
    Serial.printf("  Seriousness:%d / %d\n", pet.seriousness, SERIOUSNESS_MAX);
    SeriousnessTier tier = seriousnessSystem.getCurrentTier(pet);
    Serial.printf("  Tier:       %s\n", content::TierNames[tier]);
    Serial.printf("  Age:        Day %d", pet.age_days + 1);
    if (pet.stage == STAGE_CHILD)
        Serial.printf(" / %d (child)\n", CHILD_PERIOD_DAYS);
    else
        Serial.println(" (adult)");
    Serial.printf("  Rounds:     %d\n", deviceState.rounds);
    Serial.printf("  Visiting:   %s\n", deviceState.is_visiting ? "YES" : "NO");
    Serial.printf("  Fed today:  %d / %d\n", pet.daily_feed.feed_count, DAILY_FEED_LIMIT);
    if (pet.last_poke_effect_time == 0) {
        Serial.println("  Poke:       available");
    } else {
        uint32_t elapsed = now - pet.last_poke_effect_time;
        if (elapsed >= POKE_COOLDOWN_SEC)
            Serial.println("  Poke:       available");
        else
            Serial.printf("  Poke:       cooldown %lus left\n", POKE_COOLDOWN_SEC - elapsed);
    }
    if (pet.idle_paused_until > now)
        Serial.printf("  Idle pause: %lus left\n", pet.idle_paused_until - now);
    if (pet.mapo_tofu_count > 0)
        Serial.printf("  Mapo Tofu:  %d / %d\n", pet.mapo_tofu_count, MAPO_TOFU_CURSE_THRESHOLD);
    if (pet.is_rhongomyniad)
        Serial.println("  *** RHONGOMYNIAD ***");
    if (pet.is_black_rhongomyniad)
        Serial.println("  *** BLACK RHONGOMYNIAD (Mapo Curse) ***");
    if (pet.white_fun_form_locked && pet.alignment == ALIGN_WHITE)
        Serial.printf("  Fun form:   %s (locked)\n", content::formName(pet.white_fun_form));
    RhongoTimerState rS = seriousnessSystem.getRhongoState(pet, now);
    if (rS == RHONGO_COUNTING) {
        uint32_t rem = seriousnessSystem.getRhongoRemaining(pet, now);
        Serial.printf("  Rhongo:     %luh left\n", rem / 3600);
    } else if (rS == RHONGO_TRIGGERED) {
        Serial.println("  Rhongo:     TRIGGERED");
    }
    uint32_t wait = feedingSystem.secondsUntilNextFeed(pet, now);
    if (pet.daily_feed.feed_count >= DAILY_FEED_LIMIT)
        Serial.println("  Next feed:  LOCKED");
    else if (wait > 0)
        Serial.printf("  Next feed:  wait %lum\n", wait / 60);
    else
        Serial.println("  Next feed:  READY");
    int8_t wIdx = feedingSystem.getWindowIndex(timeManager.getHour());
    const char* wN[] = {"Breakfast", "Lunch", "Dinner"};
    if (wIdx >= 0) Serial.printf("  Window:     %s\n", wN[wIdx]);
    else Serial.println("  Window:     None");
    Serial.printf("  Gallery:    %d / %d unlocked\n",
                  gallerySystem.getData().getUnlockedCount(), content::FormCount);
    Serial.println("========================================");
}

void printStatusForSnapshot(const PetState& p, uint32_t epochNow) {
    TimeInfo ti = timeManager.epochToTimeInfo(epochNow);
    char timeBuf[24];
    snprintf(timeBuf, sizeof(timeBuf), "%04d-%02d-%02d %02d:%02d:%02d",
             ti.year, ti.month, ti.day, ti.hour, ti.minute, ti.second);

    Serial.println("========================================");
    if (p.is_nobu) {
        Serial.printf("  Time:       %s\n", timeBuf);
        Serial.println("  Name:       nobu");
        Serial.println("  HP:         ?");
        Serial.println("  SR:         ?");
        Serial.println("  Age:        ?");
        Serial.println("  Stage:      ?");
        Serial.println("  Align:      ?");
        Serial.println("========================================");
        return;
    }

    Serial.printf("  Time:       %s\n", timeBuf);
    Serial.printf("  Form:       %s\n", content::formName(p.form));
    Serial.printf("  Base:       %s\n", content::formName(p.base_form));
    Serial.printf("  Stage:      %s\n", content::StageNames[p.stage]);
    Serial.printf("  Alignment:  %s\n", ALIGNMENT_NAMES[p.alignment]);
    Serial.printf("  Health:     %d / %d\n", p.health, HEALTH_MAX);
    Serial.printf("  Seriousness:%d / %d\n", p.seriousness, SERIOUSNESS_MAX);
    SeriousnessTier tier = seriousnessSystem.getCurrentTier(p);
    Serial.printf("  Tier:       %s\n", content::TierNames[tier]);
    Serial.printf("  Age:        Day %d", p.age_days + 1);
    if (p.stage == STAGE_CHILD)
        Serial.printf(" / %d (child)\n", CHILD_PERIOD_DAYS);
    else
        Serial.println(" (adult)");
    Serial.printf("  Rounds:     %d (device)\n", deviceState.rounds);
    Serial.printf("  Fed today:  %d / %d\n", p.daily_feed.feed_count, DAILY_FEED_LIMIT);
    if (p.last_poke_effect_time == 0) {
        Serial.println("  Poke:       available");
    } else {
        uint32_t elapsed = epochNow - p.last_poke_effect_time;
        if (elapsed >= POKE_COOLDOWN_SEC)
            Serial.println("  Poke:       available");
        else
            Serial.printf("  Poke:       cooldown %lus left\n", POKE_COOLDOWN_SEC - elapsed);
    }
    if (p.idle_paused_until > epochNow)
        Serial.printf("  Idle pause: %lus left\n", p.idle_paused_until - epochNow);
    if (p.mapo_tofu_count > 0)
        Serial.printf("  Mapo Tofu:  %d / %d\n", p.mapo_tofu_count, MAPO_TOFU_CURSE_THRESHOLD);
    if (p.is_rhongomyniad)
        Serial.println("  *** RHONGOMYNIAD ***");
    if (p.is_black_rhongomyniad)
        Serial.println("  *** BLACK RHONGOMYNIAD (Mapo Curse) ***");
    if (p.white_fun_form_locked && p.alignment == ALIGN_WHITE)
        Serial.printf("  Fun form:   %s (locked)\n", content::formName(p.white_fun_form));
    RhongoTimerState rS = seriousnessSystem.getRhongoState(p, epochNow);
    if (rS == RHONGO_COUNTING) {
        uint32_t rem = seriousnessSystem.getRhongoRemaining(p, epochNow);
        Serial.printf("  Rhongo:     %luh left\n", rem / 3600);
    } else if (rS == RHONGO_TRIGGERED) {
        Serial.println("  Rhongo:     TRIGGERED");
    }
    uint32_t wait = feedingSystem.secondsUntilNextFeed(p, epochNow);
    if (p.daily_feed.feed_count >= DAILY_FEED_LIMIT)
        Serial.println("  Next feed:  LOCKED");
    else if (wait > 0)
        Serial.printf("  Next feed:  wait %lum\n", wait / 60);
    else
        Serial.println("  Next feed:  READY");
    int8_t wIdx = feedingSystem.getWindowIndex(ti.hour);
    const char* wN[] = {"Breakfast", "Lunch", "Dinner"};
    if (wIdx >= 0) Serial.printf("  Window:     %s\n", wN[wIdx]);
    else Serial.println("  Window:     None");
    Serial.printf("  Gallery:    %d / %d unlocked\n",
                  gallerySystem.getData().getUnlockedCount(), content::FormCount);
    Serial.println("========================================");
}

void printHelp() {
    Serial.printf("=== %s Console ===\n",content::Title);
    Serial.println("  s              Status");
    Serial.println("  fl             Food list");
    Serial.println("  t <min>        Advance N minutes");
    Serial.println("  d              Advance 1 day");
    Serial.println("  save / load / erase");
    Serial.println("  s0 / s1 / s2  Show per-slot snapshot status");
    Serial.println("  SAVE_SLOT_STATUS");
    Serial.println("  SAVE_EXPORT <slot>       Export full backup (pet + gallery)");
    Serial.println("  SAVE_EXPORT_PET <slot>   Export pet only (for visit/trade)");
    Serial.println("  SAVE_IMPORT_BEGIN <slot>");
    Serial.println("  SAVE_IMPORT_DATA <hex>");
    Serial.println("  SAVE_IMPORT_COMMIT / SAVE_IMPORT_ABORT");
    Serial.println("  SAVE_EXPORT_OWNER         Export retained owner recovery copy");
    Serial.println("  SAVE_RECOVERY_ERASE CONFIRM (recovery lock only; deletes all saved data)");
    Serial.println("  reset          Destroy & reset (ends visit if visiting)");
    Serial.println("  stime Y M D H m");
    Serial.println("  SET_TIME <epoch>  Set system time (unix timestamp)");
    Serial.println("  hp/sr/age <val>  Debug set");
    Serial.println("  grad           Force graduation");
    if(content::LegacyUI)Serial.println("  mapo           Debug +1 mapo count");
    if(content::LegacyUI)Serial.println("  FORCE_NOBU     Force nobu route");
    Serial.println("--- Gallery ---");
    Serial.println("  UNLOCK_ALL     Unlock all gallery forms");
    Serial.println("  RESET_GALLERY  Reset gallery (lock all)");
    Serial.println("--- Device ---");
    Serial.println("  devinfo        Show device state");
    Serial.println("--- Power ---");
    Serial.println("  bright <0-15>  Set screen brightness");
    Serial.println("  dim <0-15>     Set dim brightness");
    Serial.println("  dim_t <sec>    Set dim timeout");
    Serial.println("  off_t <sec>    Set off timeout");
    Serial.println("  pwrsave        Save power config to NVS (bright/dim/dim_t/off_t 已自动保存)");
    Serial.println("  pwrinfo        Print power config");
#if ENABLE_SERIAL_INPUT_DEBUG
    Serial.println("--- Button Simulation ---");
    Serial.println("  btn l|m|r      Simulate short press");
    Serial.println("  btnl l|m|r     Simulate long press");
    Serial.println("  btnr l|m|r     Simulate repeat");
    Serial.println("  ctx            Show current UI context");
#endif
    Serial.println("  h              Help");
    Serial.println("===============================");
}

void processCommand(const char* cmd) {
    while (*cmd == ' ') cmd++;
    if (strlen(cmd) == 0) return;

    processElapsedTime();
    if (strcmp(cmd, "SAVE_SLOT_STATUS") == 0) {
        for (uint8_t slot = 0; slot < SAVE_SLOT_COUNT; slot++) {
            SaveHeader hdr;
            SaveResult st = SAVE_OK;
            if (saveManager.getSlotStatus(slot, hdr, st)) {
                Serial.printf("slot%u:seq=%lu,time=%lu,ver=%u,size=%u,crc=0x%04X\n",
                              slot,
                              (unsigned long)hdr.sequence,
                              (unsigned long)hdr.save_time,
                              (unsigned)hdr.version,
                              (unsigned)hdr.data_size,
                              (unsigned)hdr.checksum);
            } else {
                Serial.printf("slot%u:ERR(%d)\n", slot, (int)st);
            }
        }
        return;
    }

    const bool alignClock=strcmp(cmd,"SAVE_IMPORT_COMMIT")==0&&waitingForTimeSet&&deviceState.device_clock_epoch>0;
    TimeInfo anchor{};uint32_t importNow=timeManager.now();uint8_t importDay=timeManager.getDay();
    if(alignClock){
        anchor=timeManager.epochToTimeInfo(deviceState.device_clock_epoch);
        importNow=timeManager.timeInfoToEpoch(anchor.year,anchor.month,anchor.day,anchor.hour,anchor.minute,0);
        importDay=anchor.day;
    }
    const auto protocol=app::saveProtocol(g_slotImport,cmd,saveManager,pet,deviceState,
        gallerySystem.getData(),importNow,importDay,millis());
    if(protocol==app::ProtocolResult::Erased){
        deviceState.init();deviceState.rounds=1;
        app::initializePet(pet,timeManager.now());gallerySystem.init();
        waitingForTimeSet=true;reinitPetAfterInitialTimeConfirm=true;
        menuController.startInitialTimeSetup(timeManager.now());return;
    }
    if(protocol==app::ProtocolResult::Imported){
        if(alignClock)timeManager.setSimulatedTime(anchor.year,anchor.month,anchor.day,anchor.hour,anchor.minute);
        reinitPetAfterInitialTimeConfirm=false;
        waitingForTimeSet=false;
        menuController.onPetImported();
        return;
    }
    if(protocol==app::ProtocolResult::Handled)return;
    if(saveManager.recoveryRequired() && strcmp(cmd,"s") && strcmp(cmd,"h") && strcmp(cmd,"devinfo")) {
        Serial.println("[Main] Recovery required; automatic writes and gameplay blocked. Export saved data before explicit recovery.");return;
    }


    // SET_TIME 命令: 设置系统时间 (离线补偿用, 任何状态下可用)
    if (strncmp(cmd, "SET_TIME ", 9) == 0) {
        uint32_t timestamp = strtoul(cmd + 9, nullptr, 10);
        if (timestamp < 1000000000UL) {
            Serial.println("[Time] Invalid timestamp (too small). Use unix epoch seconds.");
            return;
        }
        TimeInfo t = timeManager.epochToTimeInfo(timestamp);
        Serial.printf("[Time] Requested system time: %04d-%02d-%02d %02d:%02d:%02d\n",
                      t.year, t.month, t.day, t.hour, t.minute, t.second);

        if (waitingForTimeSet) {
            // Use the same protected setup/compensation path as the real UI.
            gameCallbacks.onInitialTimeConfirm(t.year,t.month,t.day,t.hour,t.minute);
        } else {
            if (deviceState.is_visiting) {
                Serial.println("[Time] REJECTED: cannot change time during visit.");
                return;
            }
            if(saveManager.beginImport(0xff)!=SAVE_OK){Serial.println("[Time] FAILED: protection marker");return;}
            timeManager.setSimulatedTime(t.year,t.month,t.day,t.hour,t.minute);
            timeManager.advanceSeconds(t.second);
            deviceState.device_clock_epoch=timestamp;
            if(persistDeviceState()!=SAVE_OK || saveManager.save(pet,timestamp)!=SAVE_OK){
                saveManager.requireRecovery();Serial.println("[Time] FAILED: checkpoint; recovery required");return;
            }
            if(saveManager.finishImport()!=SAVE_OK)return;
            saveManager.markSaved(timestamp);
            Serial.println("[Time] System time committed.");
        }
        return;
    }

    // 等待时间设置期间, 只允许时间设置相关和 s/h 命令
    if (waitingForTimeSet) {
        if (strcmp(cmd, "s") == 0) { printStatus(); return; }
        if (strcmp(cmd, "s0") == 0 || strcmp(cmd, "s1") == 0 || strcmp(cmd, "s2") == 0) {
            uint8_t slot = (uint8_t)(cmd[1] - '0');
            SaveHeader hdr;
            PetState snap;
            SaveResult sr = saveManager.exportSlotRaw(slot, hdr, snap);
            if (sr != SAVE_OK) {
                Serial.printf("[SlotStatus] slot%u unavailable (err=%d)\n", slot, (int)sr);
                return;
            }
            Serial.printf("[SlotStatus] slot%u seq=%lu save_time=%lu\n",
                          slot, (unsigned long)hdr.sequence, (unsigned long)hdr.save_time);
            printStatusForSnapshot(snap, hdr.save_time);
            return;
        }
        if (strcmp(cmd, "h") == 0) { printHelp(); return; }
        Serial.println("[Main] Waiting for date/time setup. Other commands blocked.");
        return;
    }

    if (strcmp(cmd, "s") == 0) { printStatus(); return; }
    if (strcmp(cmd, "s0") == 0 || strcmp(cmd, "s1") == 0 || strcmp(cmd, "s2") == 0) {
        uint8_t slot = (uint8_t)(cmd[1] - '0');
        SaveHeader hdr;
        PetState snap;
        SaveResult sr = saveManager.exportSlotRaw(slot, hdr, snap);
        if (sr != SAVE_OK) {
            Serial.printf("[SlotStatus] slot%u unavailable (err=%d)\n", slot, (int)sr);
            return;
        }
        Serial.printf("[SlotStatus] slot%u seq=%lu save_time=%lu\n",
                      slot, (unsigned long)hdr.sequence, (unsigned long)hdr.save_time);
        printStatusForSnapshot(snap, hdr.save_time);
        return;
    }
    if (strcmp(cmd, "h") == 0) { printHelp(); return; }
    if (strcmp(cmd, "fl") == 0) {
        Serial.println("--- Normal Food ---");
        for (uint8_t i = 0; i < FOOD_COUNT; i++)
            Serial.printf("  %d: %-16s %s (%+d hp)\n", i, FOOD_TABLE[i].name,
                          FOOD_TABLE[i].is_healthy ? "[H]" : "[J]", FOOD_TABLE[i].health_delta);
        Serial.println("--- Special Food ---");
        for (uint8_t i = 0; i < SFOOD_COUNT; i++)
            Serial.printf("  %d: %-16s %s\n", i, SPECIAL_FOOD_TABLE[i].name,
                          SPECIAL_FOOD_TABLE[i].description);
        return;
    }
    if (strcmp(cmd, "d") == 0) {
        if (deviceState.is_visiting) {
            Serial.println("[Time] REJECTED: cannot advance day during visit.");
            return;
        }
        if(saveManager.beginImport(0xff)!=SAVE_OK){Serial.println("[DayEnd] FAILED: protection marker");return;}
        doDayEnd();
        if(saveManager.recoveryRequired())return;
        timeManager.advanceDays(1);
        if(saveManager.save(pet,timeManager.now())!=SAVE_OK){saveManager.requireRecovery();Serial.println("[DayEnd] FAILED: final save; recovery required");return;}
        if(saveManager.finishImport()!=SAVE_OK)return;
        saveManager.markSaved(timeManager.now());
        DisplayManager::showToast("Day advanced", 1000);
        timeManager.checkNewDay();
        return;
    }
    if (strcmp(cmd, "save") == 0) {
        uint32_t ts = timeManager.now();
        SaveResult r = saveManager.save(pet, ts);
        if (r == SAVE_OK) {
            saveManager.markSaved(ts);
            Serial.println("[Save] OK");
            DisplayManager::showToast("Saved", 1000);
        } else {
            Serial.println("[Save] FAILED");
            DisplayManager::showToast("Save failed!", 2000);
        }
        return;
    }
    if (strcmp(cmd, "load") == 0) {
        if (saveManager.load(pet) == SAVE_OK) {
            restoreLoadedClock();
            Serial.println("[Load] OK");
            DisplayManager::showToast("Loaded", 1000);
            printStatus();
        } else {
            Serial.println("[Load] FAILED");
            DisplayManager::showToast("Load failed!", 2000);
        }
        return;
    }
    if (strcmp(cmd, "erase") == 0) {
        saveManager.erase();
        Serial.println("[Save] Erased");
        DisplayManager::showToast("Save erased", 1500);
        return;
    }
    if (strcmp(cmd, "reset") == 0) { doReset(); return; }
    if (strcmp(cmd, "FORCE_NOBU") == 0) {
        if(!content::LegacyUI){Serial.println("[Content] Unsupported command for this package");return;}
        uint32_t now = timeManager.now();
            evolutionSystem.destroyToNobu(pet, now, pet.age_days);
        deviceState.rounds++;
        persistDeviceState();
        feedingSystem.resetDaily(pet, timeManager.getDay());
        saveManager.save(pet, now);
        saveManager.markSaved(now);
        Serial.println("[Debug] 已强制切换为 Nobu");
        printStatus();
        return;
    }
    if (strcmp(cmd, "UNLOCK_ALL") == 0) {
        gallerySystem.unlockAll();
        if(saveManager.saveGallery(gallerySystem.getData())!=SAVE_OK){Serial.println("[Gallery] FAILED: pending retry");return;}
        if (gallerySystem.getData().getUnlockedCount() >= content::FormCount) {
            DisplayManager::showGalleryCompleteUnlocked();
        }
        Serial.println("[Debug] All gallery forms unlocked and saved");
        return;
    }
    if (strcmp(cmd, "RESET_GALLERY") == 0) {
        gallerySystem.resetGallery();
        if(saveManager.saveGallery(gallerySystem.getData())!=SAVE_OK){Serial.println("[Gallery] FAILED: pending retry");return;}
        Serial.println("[Debug] Gallery reset (all forms locked) and saved");
        return;
    }
    if (strcmp(cmd, "devinfo") == 0) {
        Serial.println("--- Device State ---");
        Serial.printf("  Rounds:       %u\n", deviceState.rounds);
        Serial.printf("  Visiting:     %s\n", deviceState.is_visiting ? "YES" : "NO");
        Serial.printf("  Clock epoch:  %lu\n", (unsigned long)deviceState.device_clock_epoch);
#if defined(ARDUINO_ARCH_ESP32)
        // On-device acceptance evidence; observations only, no rule/state changes.
        Serial.printf("  Memory: heap=%u min_heap=%u max_alloc=%u psram=%u free_psram=%u stack_bytes=%u\n",
                      ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap(),
                      ESP.getPsramSize(), ESP.getFreePsram(),
                      (unsigned)uxTaskGetStackHighWaterMark(nullptr));
#endif
        if (deviceState.is_visiting) {
            Serial.printf("  Owner slot:   %u\n", deviceState.owner_frozen_slot);
            Serial.printf("  Visit start:  %lu\n", (unsigned long)deviceState.visit_start_epoch);
        }
        Serial.println("--------------------");
        return;
    }
    if (strcmp(cmd, "grad") == 0) {
        if (pet.stage != STAGE_CHILD) { Serial.println("[Debug] Already adult."); return; }
        pet.age_days = CHILD_PERIOD_DAYS;
        EvolutionResult r = evolutionSystem.checkChildGraduation(pet);
        Serial.printf("[Debug] Grad: %s\n", EVO_EVENT_NAMES[r.event]);
        if (r.event == EVO_CHILD_TO_WHITE || r.event == EVO_CHILD_TO_BLACK) {
            DisplayManager::showChildGraduation(r, pet.alignment);
            persistGalleryUnlockFromEvolution(r);
        }
        EvolutionResult r2 = evolutionSystem.check(pet, timeManager.now());
        if (r2.event != EVO_NONE) {
            Serial.printf("[Debug] Form: %s\n", content::formName(r2.form_after));
            persistGalleryUnlockFromEvolution(r2);
        }
        printStatus();
        return;
    }
    if (strcmp(cmd, "mapo") == 0) {
        if(!content::LegacyUI){Serial.println("[Content] Unsupported command for this package");return;}
        pet.mapo_tofu_count++;
        Serial.printf("[Debug] Mapo Tofu: %d / %d\n", pet.mapo_tofu_count, MAPO_TOFU_CURSE_THRESHOLD);
        DisplayManager::showMapoTofuTriggered(pet.mapo_tofu_count, MAPO_TOFU_CURSE_THRESHOLD);
        if (pet.mapo_tofu_count >= MAPO_TOFU_CURSE_THRESHOLD) {
            EvolutionResult eR = evolutionSystem.checkMapoCurse(pet);
            if (eR.event == EVO_BLACK_RHONGOMYNIAD) {
                DisplayManager::showEvolutionEvent(eR, pet.seriousness);
                persistGalleryUnlockFromEvolution(eR);
                uint32_t ts = timeManager.now();
                saveManager.save(pet, ts);
                saveManager.markSaved(ts);
            }
        }
        return;
    }

    // t <min>
    if (cmd[0] == 't' && cmd[1] == ' ') {
        if (deviceState.is_visiting) {
            Serial.println("[Time] REJECTED: cannot advance time during visit.");
            return;
        }
        char* end=nullptr;
        const unsigned long requested=strtoul(cmd+2,&end,10);
        if(end==cmd+2 || *end || cmd[2]=='-' || requested==0 || requested>10080 || requested>(UINT32_MAX-timeManager.now())/60u) {
            Serial.println("[Time] REJECTED: supported advance is 1..10080 minutes within epoch capacity; unchanged.");return;
        }
        const auto minutes=uint32_t(requested);
        {
            const bool wasRhongoT=pet.is_rhongomyniad;
            const Form formBeforeT=pet.form;
            IdleTickResult iR{};
            iR.seriousness_before=pet.seriousness;iR.tier_before=seriousnessSystem.getCurrentTier(pet);
            for(uint32_t remaining=minutes;remaining;) {
                const auto batch=remaining>engine::MaxAdvanceMinutes?engine::MaxAdvanceMinutes:remaining;
                timeManager.advanceMinutes(batch);
                seriousnessSystem.onIdleBatch(pet,batch,timeManager.now());remaining-=batch;
            }
            const auto now=timeManager.now();
            iR.seriousness_after=pet.seriousness;iR.tier_after=seriousnessSystem.getCurrentTier(pet);
            iR.tier_changed=iR.tier_before!=iR.tier_after;
            DisplayManager::showToast("Time advanced",1000);
            Serial.printf("[Idle] SR: %d->%d (+%dm)\n", iR.seriousness_before, iR.seriousness_after, minutes);
            if (!wasRhongoT && pet.is_rhongomyniad) {
                showWhiteRhongoJustEntered(formBeforeT, pet.seriousness);
            }
            if (iR.tier_changed) {
                Serial.printf("[Idle] Tier: %s -> %s\n", content::TierNames[iR.tier_before], content::TierNames[iR.tier_after]);
                DisplayManager::showIdleTierChange(iR.tier_before, iR.tier_after);
            }
            EvolutionResult eR = evolutionSystem.check(pet, now);
            if (eR.event != EVO_NONE) {
                Serial.printf("[Idle] Evo: %s\n", EVO_EVENT_NAMES[eR.event]);
                DisplayManager::showEvolutionEvent(eR, pet.seriousness);
                persistGalleryUnlockFromEvolution(eR);
            }
        }
        return;
    }

    // stime Y M D H m
    if (strncmp(cmd, "stime ", 6) == 0) {
        if (deviceState.is_visiting) {
            Serial.println("[Time] REJECTED: cannot change time during visit.");
            return;
        }
        int y, mo, da, h, mi;
        if (sscanf(cmd + 6, "%d %d %d %d %d", &y, &mo, &da, &h, &mi) == 5) {
            timeManager.setSimulatedTime(y, mo, da, h, mi);
            DisplayManager::showToast("Time set", 1000);
        }
        else Serial.println("[Time] Usage: stime YYYY MM DD HH mm");
        return;
    }

    // hp <val>
    if (strncmp(cmd, "hp ", 3) == 0) {
        int val = atoi(cmd + 3);
        if (val >= HEALTH_MIN && val <= HEALTH_MAX) {
            int16_t old = pet.health; pet.health = val;
            Serial.printf("[Debug] HP: %d->%d\n", old, pet.health);
        }
        return;
    }

    // sr <val>
    if (strncmp(cmd, "sr ", 3) == 0) {
        int val = atoi(cmd + 3);
        if (val >= SERIOUSNESS_MIN && val <= SERIOUSNESS_MAX) {
            int16_t old = pet.seriousness; pet.seriousness = val;
            Serial.printf("[Debug] SR: %d->%d | %s\n", old, pet.seriousness,
                          content::TierNames[seriousnessSystem.getCurrentTier(pet)]);
            EvolutionResult eR = evolutionSystem.check(pet, timeManager.now());
            if (eR.event != EVO_NONE)
                Serial.printf("[Debug] Evo: %s -> %s\n", content::formName(eR.form_before), content::formName(eR.form_after));
        }
        return;
    }

    // age <val>
    if (strncmp(cmd, "age ", 4) == 0) {
        int val = atoi(cmd + 4);
        if (val >= 0) {
            uint16_t old = pet.age_days; pet.age_days = val;
            Serial.printf("[Debug] Age: %d->%d\n", old, pet.age_days);
        }
        return;
    }

    // ========================================================================
    //  Power management commands
    // ========================================================================
    if (strncmp(cmd, "bright ", 7) == 0) {
        int val = atoi(cmd + 7);
        if (val >= 0 && val <= 15) {
            powerManager.setBrightness((uint8_t)val);
            powerManager.onUserActivity();
        } else {
            Serial.println("[Power] Usage: bright <0-15>");
        }
        return;
    }
    if (strncmp(cmd, "dim ", 4) == 0 && strncmp(cmd, "dim_t ", 6) != 0) {
        int val = atoi(cmd + 4);
        if (val >= 0 && val <= 15) {
            powerManager.setDimBrightness((uint8_t)val);
        } else {
            Serial.println("[Power] Usage: dim <0-15>");
        }
        return;
    }
    if (strncmp(cmd, "dim_t ", 6) == 0) {
        uint32_t val = strtoul(cmd + 6, nullptr, 10);
        powerManager.setDimTimeout(val);
        powerManager.onUserActivity();
        return;
    }
    if (strncmp(cmd, "off_t ", 6) == 0) {
        uint32_t val = strtoul(cmd + 6, nullptr, 10);
        powerManager.setOffTimeout(val);
        powerManager.onUserActivity();
        return;
    }
    if (strcmp(cmd, "pwrsave") == 0) {
        powerManager.saveConfig();
        return;
    }
    if (strcmp(cmd, "pwrinfo") == 0) {
        Serial.println("--- Power Config ---");
        Serial.printf("  Brightness:    %d / 15\n", powerManager.getBrightness());
        Serial.printf("  Dim brightness:%d / 15\n", powerManager.getDimBrightness());
        Serial.printf("  Dim timeout:   %lu sec\n", powerManager.getDimTimeout());
        Serial.printf("  Off timeout:   %lu sec\n", powerManager.getOffTimeout());
        const char* stateNames[] = {"ACTIVE", "DIM", "OFF"};
        Serial.printf("  State:         %s\n", stateNames[powerManager.getState()]);
        Serial.println("--------------------");
        return;
    }

    // ========================================================================
    //  Button simulation (serial debug, guarded by macro)
    // ========================================================================
#if ENABLE_SERIAL_INPUT_DEBUG
    if (strcmp(cmd, "ctx") == 0) {
        Serial.printf("[MC] Context: %s\n", UI_CONTEXT_NAMES[menuController.getCurrentContext()]);
        Serial.printf("[MC] Presentation: blocking=%u playing=%u waiting=%u\n",
                      DisplayManager::isPageBlockingInput(),animQueue.isPlaying(),animQueue.isWaitingInput());
        return;
    }

    if (strncmp(cmd, "btn ", 4) == 0) {
        ButtonId btn_id;
        const char* arg = cmd + 4;
        if (strcmp(arg, "l") == 0) btn_id = BTN_L;
        else if (strcmp(arg, "m") == 0) btn_id = BTN_M;
        else if (strcmp(arg, "r") == 0) btn_id = BTN_R;
        else { Serial.println("[Btn] Usage: btn l|m|r"); return; }
        Serial.printf("[Btn] Inject PRESS %s (ctx=%s)\n", BTN_NAMES[btn_id], UI_CONTEXT_NAMES[menuController.getCurrentContext()]);
        menuController.injectButton(btn_id, BTN_EVENT_PRESS);
        return;
    }

    if (strncmp(cmd, "btnl ", 5) == 0) {
        ButtonId btn_id;
        const char* arg = cmd + 5;
        if (strcmp(arg, "l") == 0) btn_id = BTN_L;
        else if (strcmp(arg, "m") == 0) btn_id = BTN_M;
        else if (strcmp(arg, "r") == 0) btn_id = BTN_R;
        else { Serial.println("[Btn] Usage: btnl l|m|r"); return; }
        Serial.printf("[Btn] Inject LONG_PRESS %s (ctx=%s)\n", BTN_NAMES[btn_id], UI_CONTEXT_NAMES[menuController.getCurrentContext()]);
        menuController.injectButton(btn_id, BTN_EVENT_LONG_PRESS);
        return;
    }

    if (strncmp(cmd, "btnr ", 5) == 0) {
        ButtonId btn_id;
        const char* arg = cmd + 5;
        if (strcmp(arg, "l") == 0) btn_id = BTN_L;
        else if (strcmp(arg, "m") == 0) btn_id = BTN_M;
        else if (strcmp(arg, "r") == 0) btn_id = BTN_R;
        else { Serial.println("[Btn] Usage: btnr l|m|r"); return; }
        Serial.printf("[Btn] Inject REPEAT %s (ctx=%s)\n", BTN_NAMES[btn_id], UI_CONTEXT_NAMES[menuController.getCurrentContext()]);
        menuController.injectButton(btn_id, BTN_EVENT_REPEAT);
        return;
    }
#endif

    Serial.printf("[Cmd] Unknown: '%s'\n", cmd);
}

void readSerialCommand() {
    static app::CommandLine line;
    g_slotImport.expire(millis());
    while(Serial.available())line.receive(char(Serial.read()),millis(),[](const char* cmd){processCommand(cmd);});
}

// ============================================================================
//  Setup & Loop
// ============================================================================
