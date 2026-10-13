#include "import_workflow.h"
#include "workflow_services.h"
#include "startup.h"

// 保存设备态到 NVS (便捷函数)
SaveResult persistDeviceState() {
    return app::persistDeviceChecked(saveManager,deviceState);
}

// deep sleep 计时基准（RTC 慢时钟在 deep sleep 期间仍会继续走）
static void persistUnlock(Form form) {
    const bool unlocked=gallerySystem.unlockForm(form);
    if(!unlocked && !saveManager.galleryPending())return;
    if(saveManager.saveGallery(gallerySystem.getData())!=SAVE_OK) {
        Serial.println("[Gallery] FAILED: unlock pending; next save/unlock retries before pet commit");
        return;
    }
    if(unlocked && gallerySystem.getData().getUnlockedCount()>=content::FormCount)
        DisplayManager::showGalleryCompleteUnlocked();
}
void persistGalleryUnlockFromEvolution(const EvolutionResult& r) {
    if(r.event!=EVO_NONE)persistUnlock(r.form_after);
}
void persistCurrentFormUnlock() {persistUnlock(pet.form);}

// 白狮子王: updateRhongoTimer 在 tick/批量内先置位时 check() 常拿不到 EVO_RHONGOMYNIAD。调用方须保证
// 本步刚由「非白狮终态」切入 is_rhongomyniad（与 !wasRhongo && pet.is_rhongomyniad 一致）。
void showWhiteRhongoJustEntered(Form formBefore, int16_t srAfter) {
    EvolutionResult rh = {};
    rh.event = EVO_RHONGOMYNIAD;
    rh.form_before = formBefore;
    rh.form_after = FORM_WHITE_LANCER_RHONGOMYNIAD;
    rh.tier = seriousnessSystem.getTier(srAfter);
    DisplayManager::showEvolutionEvent(rh, srAfter);
    persistGalleryUnlockFromEvolution(rh);
}

void doDayEnd() {
    if(pet.age_days==UINT16_MAX){app::trace("[DayEnd] REJECTED: age capacity reached");return;}
    Serial.println("[DayEnd] --- Processing ---");
    DisplayManager::showDayEndStart();
    uint32_t now = timeManager.now();

    // 清除 poke 暂停: 日结算模拟的是"过去一天"的时间流逝,
    // idle_paused_until 是相对于当前时间的未来时间戳,
    // 如果不清除会导致整天的 SR 增长被错误阻塞

    bool wasRhongo = pet.is_rhongomyniad;
    Form formBeforeDayEndIdle = pet.form;
    IdleTickResult iR = app::startDay(pet,now);
    bool rhongoTriggeredInDayEnd = (!wasRhongo && pet.is_rhongomyniad);
    if (iR.tier_changed) {
        Serial.printf("[DayEnd] Idle SR: %d->%d | Tier: %s -> %s\n",
                      iR.seriousness_before, iR.seriousness_after,
                      content::TierNames[iR.tier_before], content::TierNames[iR.tier_after]);
        DisplayManager::showDayEndIdleSR(iR.seriousness_before, iR.seriousness_after,
                                          iR.tier_before, iR.tier_after);
    }

    if (pet.is_rhongomyniad || pet.is_black_rhongomyniad) {
        Serial.println("[DayEnd] Terminal state reached during idle.");
        if (rhongoTriggeredInDayEnd) {
            showWhiteRhongoJustEntered(formBeforeDayEndIdle, pet.seriousness);
        }
        DisplayManager::showDayEndTerminalState();
        feedingSystem.resetDaily(pet, timeManager.getDay());
        pet.age_days++;
        uint32_t ts = timeManager.now();
        SaveResult dR = saveManager.save(pet, ts);
        if (dR == SAVE_OK) {
            saveManager.markSaved(ts);
        } else {
            saveManager.requireRecovery();
            Serial.printf("[DayEnd] FAILED: save failed (%d); recovery required\n", (int)dR);
        }
        return;
    }

    DayEndOutcome fO = feedingSystem.processDayEnd(pet);
    if (fO.window_bonus_applied) {
        Serial.printf("[DayEnd] Window bonus +%d HP\n", CORRECT_WINDOW_BONUS);
        DisplayManager::showDayEndWindowBonus(CORRECT_WINDOW_BONUS);
    }
    if (fO.window_penalty_applied) {
        Serial.printf("[DayEnd] Window penalty -%d HP\n", WRONG_WINDOW_PENALTY);
        DisplayManager::showDayEndWindowPenalty(WRONG_WINDOW_PENALTY);
    }
    if (fO.missed_feed_penalty) {
        int16_t srBefore = pet.seriousness;
        seriousnessSystem.applyMissedFeedPenalty(pet);
        Serial.printf("[DayEnd] Missed feeds (%d/%d)\n",
                      pet.daily_feed.feed_count, DAILY_FEED_LIMIT);
        DisplayManager::showDayEndMissedFeed(pet.daily_feed.feed_count, DAILY_FEED_LIMIT);
        DisplayManager::showMissedFeedPenalty(srBefore, pet.seriousness, MISSED_FEED_SERIOUSNESS);
    }

    EvolutionResult evo = app::growDay(pet);
    if (evo.event == EVO_CHILD_TO_WHITE || evo.event == EVO_CHILD_TO_BLACK) {
        Serial.printf("[DayEnd] Graduation: %s\n", EVO_EVENT_NAMES[evo.event]);
        DisplayManager::showChildGraduation(evo, pet.alignment);
        persistGalleryUnlockFromEvolution(evo);
    } else {
        // 只有非毕业情况才检查成年进??
        if (evo.event == EVO_NONE && pet.stage == STAGE_ADULT)
            evo = evolutionSystem.check(pet, now);
        if (evo.event != EVO_NONE) {
            Serial.printf("[DayEnd] Evo: %s\n", EVO_EVENT_NAMES[evo.event]);
            DisplayManager::showEvolutionEvent(evo, pet.seriousness);
            persistGalleryUnlockFromEvolution(evo);
        }
    }

    feedingSystem.resetDaily(pet, timeManager.getDay());
    Serial.printf("[DayEnd] Day %d done.\n", pet.age_days);
    DisplayManager::showDayEndComplete(pet.age_days);

    uint32_t ts = timeManager.now();
    SaveResult dR = saveManager.save(pet, ts);
    if (dR == SAVE_OK) {
        saveManager.markSaved(ts);
    } else {
        saveManager.requireRecovery();
        Serial.printf("[DayEnd] FAILED: save failed (%d); recovery required\n", (int)dR);
    }
}

void doReset() {
    const auto destroyedForm=pet.form;
    const auto result=deviceState.is_visiting
        ? app::restoreVisitOwner(saveManager,pet,deviceState,timeManager.now(),timeManager.getDay())
        : app::resetPetChecked(saveManager,pet,deviceState,gallerySystem.getData(),timeManager.now(),timeManager.getDay());
    if(result!=SAVE_OK){DisplayManager::showToast("Recovery required",3000);return;}
    DisplayManager::showDestroyExecuted(destroyedForm);
    printStatus();
}

// ============================================================================
//  离线时间补偿 - 模拟离线期间经过的天数
// ============================================================================

void skipTime(uint32_t offlineSeconds) {
    if(!app::offlineSupported(offlineSeconds,pet.age_days,timeManager.now())){
        app::trace("[Offline] REJECTED: exceeds 366 days, age or epoch capacity; state unchanged.");
        return;
    }
    if (offlineSeconds < 60) {
        Serial.println("[Offline] Less than 1 minute offline, no compensation needed.");
        return;
    }

    const bool ownGuard=!saveManager.operationInProgress();
    if(ownGuard && saveManager.beginImport(deviceState.is_visiting?deviceState.owner_frozen_slot:0xff)!=SAVE_OK){
        app::trace("[Offline] FAILED: protection marker; state unchanged.");return;
    }
    const auto plan=app::offlinePlan(offlineSeconds);
    uint32_t offlineDays=plan.days;
    uint32_t remainingMinutes=plan.minutes;

    Serial.printf("[Offline] Compensating: %lu days + %lu minutes\n", offlineDays, remainingMinutes);

    // 逐天结算 (与 d 命令等效)
    for (uint32_t i = 0; i < offlineDays; i++) {
        doDayEnd();
        if(saveManager.recoveryRequired())return;
        timeManager.advanceDays(1);
        timeManager.checkNewDay();  // 消耗 newDay 标记
    }

    // 结算剩余分钟的严肃值增长
    if (remainingMinutes > 0) {
        // 防守式恢复：若 idle_paused_until 丢失，但 poke 生效时间存在，
        // 则可推导出本次应有的暂停截止，避免离线补偿把暂停窗口算成普通增长。
        if (pet.last_poke_effect_time > 0) {
            uint32_t derivedPauseUntil = pet.last_poke_effect_time + POKE_IDLE_PAUSE_SEC;
            if (derivedPauseUntil > pet.idle_paused_until) {
                Serial.printf("[Offline] Recover idle pause: %lu -> %lu (from poke)\n",
                              pet.idle_paused_until, derivedPauseUntil);
                pet.idle_paused_until = derivedPauseUntil;
            }
        }

        bool wasRhongo = pet.is_rhongomyniad;
        Form formBeforeOfflineBatch = pet.form;
        uint32_t beforeEpoch = timeManager.now();
        Serial.printf("[Offline] Before batch: now=%lu pause_until=%lu last_poke_effect=%lu rem=%lu min\n",
                      beforeEpoch, pet.idle_paused_until, pet.last_poke_effect_time, remainingMinutes);
        timeManager.advanceMinutes(remainingMinutes);
        uint32_t now = timeManager.now();
        IdleTickResult iR = seriousnessSystem.onIdleBatch(pet, remainingMinutes, now);
        Serial.printf("[Offline] After batch: now=%lu SR %d->%d remainder=%u\n",
                      now, iR.seriousness_before, iR.seriousness_after, pet.idle_minute_remainder);
        if (!wasRhongo && pet.is_rhongomyniad) {
            showWhiteRhongoJustEntered(formBeforeOfflineBatch, pet.seriousness);
        }
        if (iR.tier_changed) {
            Serial.printf("[Offline] Tier: %s -> %s\n", content::TierNames[iR.tier_before], content::TierNames[iR.tier_after]);
            EvolutionResult eR = evolutionSystem.check(pet, now);
            if (eR.event != EVO_NONE) {
                Serial.printf("[Offline] Evo: %s -> %s\n", content::formName(eR.form_before), content::formName(eR.form_after));
                DisplayManager::showEvolutionEvent(eR, pet.seriousness);
                persistGalleryUnlockFromEvolution(eR);
            }
        }
    }

    uint32_t ts = timeManager.now();
    SaveResult oR = saveManager.save(pet, ts);
    if (oR == SAVE_OK) {
        if(ownGuard && saveManager.finishImport(deviceState.is_visiting?deviceState.owner_frozen_slot:0xff)!=SAVE_OK)return;
        saveManager.markSaved(ts);
    } else {
        saveManager.requireRecovery();
        Serial.printf("[Offline] FAILED: save failed (%d); recovery required\n", (int)oR);
        return;
    }
    Serial.printf("[Offline] Compensation complete. Age: Day %d\n",pet.age_days);
    printStatus();
}


void processElapsedTime() {
    if(waitingForTimeSet || saveManager.recoveryRequired())return;
    // 4. Time-based game logic
    uint32_t now = timeManager.now();

    // Idle tick: seriousness growth + rhongo timer
    if (timeManager.checkNewMinute()) {
        if (!pet.is_rhongomyniad && !pet.is_black_rhongomyniad) {
            Form formBeforeIdleTick = pet.form;
            IdleTickResult iR = seriousnessSystem.onIdleTick(pet, now);
            // 外层已排除白/黑狮终态，此处 is_rhongomyniad 为真即本分钟刚切入（等价 !wasRhongo && …）
            if (pet.is_rhongomyniad) {
                showWhiteRhongoJustEntered(formBeforeIdleTick, pet.seriousness);
            }
            if (iR.tier_changed) {
                Serial.printf("[Idle] Tier: %s -> %s\n", TIER_NAMES[iR.tier_before], TIER_NAMES[iR.tier_after]);
                DisplayManager::showIdleTierChange(iR.tier_before, iR.tier_after);
                EvolutionResult eR = evolutionSystem.check(pet, now);
                if (eR.event != EVO_NONE) {
                    Serial.printf("[Idle] Evo: %s\n", EVO_EVENT_NAMES[eR.event]);
                    DisplayManager::showEvolutionEvent(eR, pet.seriousness);
                    persistGalleryUnlockFromEvolution(eR);
                }
            }
        }
    }

    // Day-end check
    if (timeManager.checkNewDay()) {
        Serial.println("[Auto] New day detected, running day-end...");
        DisplayManager::showNewDayDetected();
        doDayEnd();
    }

}

void restoreLoadedClock() {
    app::adoptSnapshotClock(saveManager,deviceState);
    const auto t=timeManager.epochToTimeInfo(deviceState.device_clock_epoch);
    timeManager.setSimulatedTime(t.year,t.month,t.day,t.hour,t.minute);
    timeManager.advanceSeconds(t.second);
}

SaveResult autoSaveCurrentPet() {
    const auto now=timeManager.now();
    const auto result=saveManager.save(pet,now);
    if(result!=SAVE_OK){
        static uint32_t lastFailureMs=0;
        const auto ms=app::ports.monotonic?app::ports.monotonic(app::ports.context).value:0;
        if(uint32_t(ms-lastFailureMs)>=3000){lastFailureMs=ms;Serial.printf("[Save] FAILED: auto-save (%d)\n",int(result));}
        return result;
    }
    saveManager.markSaved(now);
    deviceState.device_clock_epoch=now;
    const auto deviceResult=persistDeviceState();
    if(deviceResult!=SAVE_OK)return deviceResult;
    DisplayManager::showAutoSave();return SAVE_OK;
}
