#include "import_workflow.h"
#include "workflow_services.h"
#include "startup.h"

UICallbacks gameCallbacks = {
    // onStatusOpen
    [](const PetState& p) {
        Serial.println("[MC] Status opened");
        DisplayManager::showStatusPanel(p);
    },
    // onStatusClose
    []() {
        Serial.println("[MC] Status closed");
        DisplayManager::hideStatusPanel();
    },
    // onFeedDrawStart
    [](const FeedDraw& draw) {
        Serial.println("[MC] Feed draw started");
        Serial.printf("[MC] Draw food IDs: %u %u %u %u\n",
                      FOOD_TABLE[draw.food_ids[0]].id, FOOD_TABLE[draw.food_ids[1]].id,
                      FOOD_TABLE[draw.food_ids[2]].id, FOOD_TABLE[draw.food_ids[3]].id);
        DisplayManager::showFeedDraw(draw);
        // Animation complete will be triggered by DisplayManager::update()
    },
    // onFeedCursorMove
    [](uint8_t cursor, const bool selected[4]) {
        Serial.printf("[MC] Feed cursor -> %d\n", cursor);
        DisplayManager::showFeedCursorMove(cursor, selected);
    },
    // onFeedSlotToggle
    [](uint8_t slot, bool sel) {
        Serial.printf("[MC] Feed slot %d %s\n", slot, sel ? "SELECTED" : "DESELECTED");
        DisplayManager::showFeedSlotToggle(slot, sel);
    },
    // onFeedConfirm (new signature: outcome + srAfter)
    [](const FeedOutcome& outcome, int16_t srAfter) {
        Serial.printf("[MC] Feed confirmed! HP: %d->%d  SR after: %d\n",
            outcome.health_before, outcome.health_after, srAfter);
        if (outcome.combo_triggered)
            Serial.printf("[MC] *** COMBO: %s ***\n", COMBO_NAMES[outcome.combo]);
        // 注意: 不再调用 showFeedResult/showFeedComboTriggered
        // 动画由 AnimationDirector::buildFeedSequence() 通过队列驱动
    },
    // onFeedCancel
    []() {
        Serial.println("[MC] Feed cancelled");
        DisplayManager::showFeedCancel();
    },
    // onSpecialFoodShow
    [](uint8_t count) {
        Serial.printf("[MC] Special food selection (%d items)\n", count);
        DisplayManager::showSpecialFoodSelection(count);
    },
    // onSpecialFoodCursor
    [](uint8_t cursor) {
        Serial.printf("[MC] Special food cursor -> %d\n", cursor);
        DisplayManager::showSpecialFoodCursor(cursor);
    },
    // onSpecialFoodSelect (new signature: id + full outcome)
    [](uint8_t id, const FeedOutcome& outcome) {
        Serial.printf("[MC] Special food selected: %d (%s)\n", id, SPECIAL_FOOD_TABLE[id].name);
        if (outcome.mapo_tofu_triggered)
            Serial.printf("[MC] !! MAPO TOFU triggered (%d/%d) !!\n",
                outcome.mapo_tofu_total, MAPO_TOFU_CURSE_THRESHOLD);
        // 注意: 不再调用 showSpecialFoodConfirm()
        // 动画由 AnimationDirector::buildComboEatingSequence() 通过队列驱动
    },
    // onPokeStart
    []() {
        Serial.println("[MC] Poke animation started");
        // 注意: 不再调用 showPokeAnimation()
        // 动画由 AnimationDirector::buildPokeSequence() 通过队列驱动
    },
    // onPokeResult
    [](bool valueChanged, int16_t srBefore, int16_t srAfter) {
        if (valueChanged)
            Serial.printf("[MC] Poke result: SR %d->%d\n", srBefore, srAfter);
        else
            Serial.println("[MC] Poke: cooldown (animation only)");
        DisplayManager::showPokeResult(valueChanged, srBefore, srAfter);
    },
    // onDestroyConfirmShow
    [](uint8_t cursor) {
        Serial.printf("[MC] Destroy confirm shown (cursor=%d)\n", cursor);
        DisplayManager::showDestroyConfirm(cursor);
    },
    // onDestroyCursorMove
    [](uint8_t cursor) {
        Serial.printf("[MC] Destroy cursor -> %d (%s)\n", cursor, cursor == 0 ? "YES" : "NO");
        DisplayManager::showDestroyCursorMove(cursor);
    },
    // onDestroyExecuted
    [](Form destroyedForm) {

        // 串门结束: 恢复主人宠物
        if (deviceState.is_visiting) {
            if(app::restoreVisitOwner(saveManager, pet, deviceState,
                                   timeManager.now(), timeManager.getDay())!=SAVE_OK){
                DisplayManager::showToast("Recovery required",3000);return;
            }
        }
        Serial.println("[MC] *** DESTROYED ***");
        DisplayManager::showDestroyExecuted(destroyedForm);
    },
    // onDestroyCancelled
    []() {
        Serial.println("[MC] Destroy cancelled");
        DisplayManager::showDestroyCancelled();
    },
    // onEvolution (new signature: result + srAfter)
    [](const EvolutionResult& r, int16_t srAfter) {
        Serial.printf("[MC] Evolution: %s -> %s (SR=%d)\n",
            content::formName(r.form_before), content::formName(r.form_after), srAfter);
        DisplayManager::showEvolutionEvent(r, srAfter);
        // 图鉴: 解锁新形态并持久化
        persistGalleryUnlockFromEvolution(r);
    },
    // onContextChange
    [](UIContext from, UIContext to) {
        Serial.printf("[MC] Context: %s -> %s\n", UI_CONTEXT_NAMES[from], UI_CONTEXT_NAMES[to]);
        // 同步 DisplayPage ??UIContext
        switch (to) {
            case UI_IDLE:
                // 动画播放中不切页?? 等动画结束后再切
                // Page hold 期间也不切页?? ??hold 结束后自动切
                if (!DisplayManager::isAnimationPlaying() && !DisplayManager::isPageHoldActive())
                    DisplayManager::switchPage(PAGE_IDLE);
                break;
            case UI_STATUS:
                DisplayManager::switchPage(PAGE_STATUS);
                break;
            case UI_FEED_PICK:
                DisplayManager::switchPage(PAGE_FEED_PICK);
                break;
            case UI_SPECIAL_FOOD:
                DisplayManager::switchPage(PAGE_SPECIAL_FOOD);
                break;
            case UI_DESTROY_CONFIRM:
                DisplayManager::switchPage(PAGE_DESTROY_CONFIRM);
                break;
            case UI_GALLERY:
                DisplayManager::switchPage(PAGE_GALLERY);
                break;
            default:
                // UI_FEED_DRAW, UI_POKE_ANIM, UI_EVOLUTION 由具??show*() 负责切页
                break;
        }
    },
    // onInitialTimeEdit
    [](uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t fieldIndex,
       bool awaitingConfirm) {
        DisplayManager::showInitialTimeSetup(year, month, day, hour, minute, fieldIndex, awaitingConfirm);
    },
    // onInitialTimeConfirm
    [](uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute) -> TimeConfirmResult {
        if(!saveManager.initialized()||saveManager.recoveryRequired()||saveManager.importPending()){
            waitingForTimeSet=true;
            app::trace("[Time] REJECTED: saved-data recovery required; time setup not committed.");return TimeConfirmResult::Recovery;
        }
        app::adoptSnapshotClock(saveManager,deviceState);
        uint32_t timestamp = timeManager.timeInfoToEpoch(year, month, day, hour, minute, 0);
        if(!reinitPetAfterInitialTimeConfirm&&deviceState.device_clock_epoch>0&&timestamp>deviceState.device_clock_epoch&&
           !app::offlineSupported(timestamp-deviceState.device_clock_epoch,pet.age_days,deviceState.device_clock_epoch)){
            app::trace("[Offline] REJECTED: time confirmation exceeds offline capacity; awaiting supported time.");
            return TimeConfirmResult::Retry;
        }
        const auto guarded=saveManager.beginImport(deviceState.is_visiting?deviceState.owner_frozen_slot:0xff);
        if(guarded!=SAVE_OK){waitingForTimeSet=true;app::trace("[Time] FAILED: protection marker; time setup not committed.");return TimeConfirmResult::Recovery;}
        waitingForTimeSet = false;

        // 离线补偿: 用上次关机前的设备时间 vs 用户确认的当前时间
        uint32_t lastDeviceClock = deviceState.device_clock_epoch;

        if (!reinitPetAfterInitialTimeConfirm && lastDeviceClock > 0 && timestamp > lastDeviceClock) {
            uint32_t offlineDuration = timestamp - lastDeviceClock;
            Serial.printf("[Offline] Duration: %lu seconds (%.1f days)\n",
                          offlineDuration, (float)offlineDuration / 86400.0f);
            // 先回到上次设备时间, 再补算离线时长
            TimeInfo base = timeManager.epochToTimeInfo(lastDeviceClock);
            timeManager.setSimulatedTime(base.year, base.month, base.day, base.hour, base.minute);
            timeManager.advanceSeconds(base.second);
            // 访客宠物正常接受离线补偿 (主人冻结在槽内不受影响)
            skipTime(offlineDuration);
        } else {
            timeManager.setSimulatedTime(year, month, day, hour, minute);
            if (!reinitPetAfterInitialTimeConfirm) {
                Serial.println("[Offline] No compensation needed (no device clock or time went backwards).");
            }
        }

        // 更新设备时间
        deviceState.device_clock_epoch = timeManager.now();
        if(persistDeviceState()!=SAVE_OK){waitingForTimeSet=true;app::trace("[Time] FAILED: device state; recovery required.");return TimeConfirmResult::Recovery;}

        if (reinitPetAfterInitialTimeConfirm) {
            app::initializePet(pet,timeManager.now());
            gallerySystem.unlockForm(pet.form);
            reinitPetAfterInitialTimeConfirm = false;
            Serial.println("[Main] Prepared new pet for initial time save.");
        }

        // 设时后立即存档
        uint32_t nowEpoch = timeManager.now();
        if(saveManager.saveGallery(gallerySystem.getData())!=SAVE_OK){saveManager.requireRecovery();waitingForTimeSet=true;app::trace("[Time] FAILED: gallery; recovery required.");return TimeConfirmResult::Recovery;}
        SaveResult r = saveManager.save(pet, nowEpoch);
        if(r!=SAVE_OK){saveManager.requireRecovery();waitingForTimeSet=true;app::trace("[Time] FAILED: final save; recovery required.");return TimeConfirmResult::Recovery;}
        if(saveManager.finishImport(deviceState.is_visiting?deviceState.owner_frozen_slot:0xff)!=SAVE_OK){waitingForTimeSet=true;app::trace("[Time] FAILED: completion marker; recovery required.");return TimeConfirmResult::Recovery;}
        saveManager.markSaved(nowEpoch);

        menuController.switchContext(UI_IDLE);
        DisplayManager::showSystemReady();
        Serial.println("[Main] Initial time confirmed, entering normal operation.");
        printStatus();
        return TimeConfirmResult::Accepted;
    }
};

// ============================================================================
//  Serial command processing
// ============================================================================
