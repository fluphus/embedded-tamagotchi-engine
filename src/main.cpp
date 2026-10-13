#include "app/startup.h"
#include "app/context.h"
#include "platform/arduino/diagnostics.h"

void setup() {
    app::configure({nullptr,
        [](void*) -> engine::EpochSeconds {return {timeManager.now()};},
        [](void*) -> engine::MonotonicMillis {return {millis()};},
        [](void*) -> uint32_t {return esp_random();},
        [](void*,const char* line) {platform::diagnosticLine(line);}});
    Serial.begin(115200);
    delay(1000);
    Serial.println();
    Serial.println("================================");
    Serial.println("  Fate Tamagotchi");
    Serial.println("  Full Game Logic + Display");
    Serial.println("================================");

    // Initialize display first
    DisplayManager::init();

    // Initialize power manager
    powerManager.init();

    timeManager.init();
    if (saveManager.init() != SAVE_OK) {
        Serial.println("[Main] WARNING: saveManager init failed; persistence may be unavailable");
    }

    // 初始化图鉴系统
    gallerySystem.init();

    esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
    bool wokeFromDeepSleep = (wakeCause != ESP_SLEEP_WAKEUP_UNDEFINED);
    Serial.printf("[Boot] Wakeup cause: %s\n", wakeupCauseName(wakeCause));

    esp_reset_reason_t resetReason = esp_reset_reason();
    Serial.printf("[Boot] Reset reason: %s\n", resetReasonName(resetReason));

    const auto startup=app::loadStartup(saveManager,pet);
    if (startup!=app::Startup::NewGame) {
        if (startup==app::Startup::Loaded) {
            Serial.println("[Main] Save loaded");
            DisplayManager::showSaveLoaded();

            // 加载设备态
            saveManager.loadDeviceState(deviceState);
            app::adoptSnapshotClock(saveManager,deviceState);
            // 自洽性: 有存档但无设备态时 rounds 不应为 0
            if (deviceState.rounds == 0) {
                deviceState.rounds = 1;
                persistDeviceState();
            }

            // 加载图鉴数据
            saveManager.loadGallery(gallerySystem.getData());
            // 确保当前形态已解锁
            gallerySystem.unlockForm(pet.form);

            // 时间恢复逻辑：
            // - deep sleep 唤醒：用 RTC 推进, 触发离线补偿
            // - 断电类复位 (POWERON/BROWNOUT)：需要用户设时 (设备态时间丢失)
            // - 非断电类复位：沿用设备态时间继续运行
            if (resetReason == ESP_RST_DEEPSLEEP || wokeFromDeepSleep) {
                uint64_t nowRtcUs = esp_rtc_get_time_us();
                uint32_t resumedEpoch = (deviceState.device_clock_epoch > 0)
                    ? deviceState.device_clock_epoch : timeManager.now();
                uint32_t sleptSec = 0;
                if (rtc_us_at_sleep != 0 && epoch_at_sleep != 0 && nowRtcUs >= rtc_us_at_sleep) {
                    sleptSec = (uint32_t)((nowRtcUs - rtc_us_at_sleep) / 1000000ULL);
                    resumedEpoch = epoch_at_sleep + sleptSec;
                    Serial.printf("[Boot] Deep sleep elapsed: %lu sec\n", sleptSec);
                } else {
                    Serial.println("[Boot] Deep sleep markers missing; falling back to device clock.");
                }

                if (resumedEpoch > deviceState.device_clock_epoch) {
                    const auto duration=resumedEpoch-deviceState.device_clock_epoch;
                    restoreLoadedClock();
                    // 访客与非串门一致走 skipTime；串门期间: 访客宠物正常接受离线补偿 主人仍在冻结槽内，不会被改写
                    skipTime(duration);
                } else {
                    TimeInfo t = timeManager.epochToTimeInfo(resumedEpoch);
                    timeManager.setSimulatedTime(t.year, t.month, t.day, t.hour, t.minute);
                    timeManager.advanceSeconds(t.second);
                }

                // 更新设备时间
                deviceState.device_clock_epoch = timeManager.now();
                persistDeviceState();

                waitingForTimeSet = saveManager.recoveryRequired();
                powerManager.onWakeFromSleep();
                Serial.println("[Boot] Resumed (deep sleep): RTC-based time restore.");
            } else if (resetReason == ESP_RST_POWERON || resetReason == ESP_RST_BROWNOUT) {
                if (deviceState.device_clock_epoch > 0) {
                    // 有设备时间记录: 需要用户确认当前时间以计算离线时长
                    waitingForTimeSet = true;
                    reinitPetAfterInitialTimeConfirm = false;
                    Serial.println("[Main] Power-loss boot: please set current date/time via UI.");
                } else {
                    // 全新设备, 无时间记录
                    waitingForTimeSet = true;
                    reinitPetAfterInitialTimeConfirm = false;
                    Serial.println("[Main] No device clock: please set date/time.");
                }
            } else {
                // The loaded atomic snapshot defines time and already-settled progress.
                restoreLoadedClock();
                waitingForTimeSet = false;
                reinitPetAfterInitialTimeConfirm = false;
                Serial.println("[Boot] Non-power reset: restored to loaded snapshot time.");
            }
        } else {
            Serial.println("[Main] Save recovery required: existing records preserved; automatic writes blocked.");
            DisplayManager::showSaveCorruptedNewGame();
            app::initializePet(pet,timeManager.now());
            saveManager.loadDeviceState(deviceState);
            if (deviceState.rounds == 0) {
                deviceState.rounds = 1;
                persistDeviceState();
            }
            waitingForTimeSet = true;
            reinitPetAfterInitialTimeConfirm = true;
        }
    } else {
        Serial.println("[Main] New game");
        DisplayManager::showNewGame();
        app::initializePet(pet,timeManager.now());
        // 初始化设备态
        deviceState.init();
        deviceState.rounds = 1;
        // 新游戏: 解锁初始形态
        gallerySystem.unlockForm(pet.form);
        waitingForTimeSet = true;
        reinitPetAfterInitialTimeConfirm = true;
    }

    menuController.init(&pet, &deviceState, &gameCallbacks);
    Serial.println("[Main] MenuController initialized");

    printHelp();
    printStatus();

    if (waitingForTimeSet) {
        uint32_t baseEpoch = (deviceState.device_clock_epoch > 0)
            ? deviceState.device_clock_epoch
            : timeManager.timeInfoToEpoch(2026, 1, 1, 0, 0, 0);
        menuController.startInitialTimeSetup(baseEpoch);
        Serial.println("\n[Main] Waiting for date/time setup (buttons)...\n> ");
    } else {
        DisplayManager::showSystemReady();
        Serial.println("\nReady.\n> ");
    }
}

// ============================================================================
//  Deep Sleep 进入 (保持内部计时)
// ============================================================================

void loop() {
    engine::Event event;
    while(app::runtime.poll(event)) { /* snapshots already handed to presentation facade */ }
    processElapsedTime();
    // 1. Serial commands (always active, even during time-set wait)
    readSerialCommand();

    // 等待时间设置期间, 只处理串口命令和显示更新
    if (waitingForTimeSet || saveManager.recoveryRequired()) {
        menuController.update();
        powerManager.update(millis());
        DisplayManager::update(millis());
        delay(10);
        return;
    }

    // 2. Power management update
    powerManager.update(millis());

    uint32_t hwNow = millis();

    // 3. 输入在深睡尝试之前：熄屏后若入睡失败，重试间隙内用户可按键亮屏并取消重试
    menuController.update();

    bool firstDeepSleep = powerManager.shouldEnterDeepSleep();
    bool retryDeepSleep = (powerManager.getState() == POWER_OFF &&
                           powerManager.isDeepSleepRetryDue(hwNow));
    if (firstDeepSleep || retryDeepSleep) {
        if (firstDeepSleep) {
            powerManager.clearSleepFlag();
        }
        powerManager.cancelDeepSleepRetry();
        enterDeepSleep();
        if (powerManager.getState() == POWER_OFF) {
            powerManager.scheduleDeepSleepRetry(millis());
        }
        return;
    }

    uint32_t now=timeManager.now();

    // Auto-save
    if (saveManager.shouldAutoSave(now)) {
        autoSaveCurrentPet();
    }

    // 5. Display update (state machine + render)
    DisplayManager::updatePetSnapshot(pet);
    DisplayManager::setDeviceInfo(deviceState.rounds, deviceState.is_visiting);
    DisplayManager::update(millis());

    delay(10);
}
