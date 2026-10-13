#include "../../app/context.h"

const char* wakeupCauseName(esp_sleep_wakeup_cause_t c) {
    switch (c) {
        case ESP_SLEEP_WAKEUP_EXT0: return "EXT0";
        case ESP_SLEEP_WAKEUP_EXT1: return "EXT1";
        case ESP_SLEEP_WAKEUP_TIMER: return "TIMER";
        case ESP_SLEEP_WAKEUP_TOUCHPAD: return "TOUCHPAD";
        case ESP_SLEEP_WAKEUP_ULP: return "ULP";
        case ESP_SLEEP_WAKEUP_GPIO: return "GPIO";
        case ESP_SLEEP_WAKEUP_UART: return "UART";
        case ESP_SLEEP_WAKEUP_WIFI: return "WIFI";
        case ESP_SLEEP_WAKEUP_COCPU: return "COCPU";
        case ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG: return "COCPU_TRAP";
        case ESP_SLEEP_WAKEUP_BT: return "BT";
        default: return "UNDEFINED";
    }
}

const char* resetReasonName(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_POWERON: return "POWERON";
        case ESP_RST_EXT: return "EXT";
        case ESP_RST_SW: return "SW";
        case ESP_RST_PANIC: return "PANIC";
        case ESP_RST_INT_WDT: return "INT_WDT";
        case ESP_RST_TASK_WDT: return "TASK_WDT";
        case ESP_RST_WDT: return "WDT";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
        case ESP_RST_BROWNOUT: return "BROWNOUT";
        case ESP_RST_SDIO: return "SDIO";
        default: return "UNKNOWN";
    }
}

void enterDeepSleep() {
    // 入睡前存档：每次尝试先采一次游戏时间 nowEpoch，save 与 verify 共用该值（验证的是「本笔写入的时间戳」，非 verify 时刻再读时钟）
    // 写入失败可在粘滞目标重试至多3次；提交后读回验证失败立即停止，不轮转到另一槽。
    const int SLEEP_SAVE_ATTEMPTS = 3;
    bool sleepSaveVerified = false;
    uint32_t verifiedEpoch = 0;
    for (int attempt = 0; attempt < SLEEP_SAVE_ATTEMPTS; attempt++) {
        uint32_t nowEpoch = timeManager.now();  // 与本轮 save / verifyLatestSave 绑定，避免延迟导致时间比对误判
        SaveResult r = saveManager.save(pet, nowEpoch);
        if (r != SAVE_OK) {
            Serial.printf("[Sleep] pre-sleep save failed (try %d/%d, err=%d)\n",
                          attempt + 1, SLEEP_SAVE_ATTEMPTS, (int)r);
            continue;
        }
        if (!saveManager.verifyLatestSave(nowEpoch, pet)) {
            Serial.printf("[Sleep] pre-sleep save verify failed (try %d/%d)\n",
                          attempt + 1, SLEEP_SAVE_ATTEMPTS);
            // A committed record could not be read back: do not rotate over
            // another valid slot in an attempt to hide that verification fault.
            saveManager.requireRecovery();
            Serial.println("[Sleep] ABORT: committed save verification failed; not entering deep sleep");
            return;
        }
        sleepSaveVerified = true;
        verifiedEpoch = nowEpoch;
        saveManager.markSaved(nowEpoch);
        break;
    }
    if (!sleepSaveVerified) {
        Serial.println("[Sleep] ABORT: save not verified; not entering deep sleep");
        Serial.flush();
        return;
    }

    // 保存设备时间到设备态
    deviceState.device_clock_epoch = verifiedEpoch;
    if(persistDeviceState()!=SAVE_OK) {
        Serial.println("[Sleep] ABORT: device state not persisted; recovery required; not entering deep sleep");
        Serial.flush();return;
    }

    Serial.println("[Power] Entering deep sleep...");
    Serial.flush();

    // 记录 deep sleep 基准：RTC us + epoch（与已通过校验的那次 save 的 saveTime 一致）
    epoch_at_sleep = verifiedEpoch;
    rtc_us_at_sleep = esp_rtc_get_time_us();

    // 确保 RTC 外设域保持供电，否则 RTC pullup 可能在 deep sleep 期间失效，导致 EXT1(ANY_LOW) 秒醒
    esp_err_t sleepCfg = esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    if (sleepCfg != ESP_OK) {
        Serial.printf("[Sleep] esp_sleep_pd_config failed: %s\n", esp_err_to_name(sleepCfg));
    }

    // 防止“刚入睡就立刻唤醒”：
    // EXT1(ANY_LOW) 在入睡瞬间如果检测到唤醒脚为低电平会马上唤醒。
    // 运行态的 INPUT_PULLUP 在 deep sleep 下不一定保持，因此这里为可用的 RTC GPIO 显式开启 RTC 上拉。
    const gpio_num_t wakePins[] = {
        (gpio_num_t)PIN_BTN_L,
        (gpio_num_t)PIN_BTN_M,
        (gpio_num_t)PIN_BTN_R,
    };
    for (gpio_num_t pin : wakePins) {
        int level = digitalRead((int)pin);
        Serial.printf("[Sleep] Wake pin GPIO%d level=%d rtc_valid=%d\n",
                      (int)pin, level, rtc_gpio_is_valid_gpio(pin) ? 1 : 0);
        if (rtc_gpio_is_valid_gpio(pin)) {
            rtc_gpio_pulldown_dis(pin);
            rtc_gpio_pullup_en(pin);
            rtc_gpio_hold_en(pin);
        }
    }

    // 配置唤醒源: 任意按键 (GPIO) 唤醒
    uint64_t wakeupMask = 0;
    wakeupMask |= (1ULL << PIN_BTN_L);
    wakeupMask |= (1ULL << PIN_BTN_M);
    wakeupMask |= (1ULL << PIN_BTN_R);
    const int EXT1_WAKEUP_RETRIES = 8;
    for (int attempt = 0; attempt < EXT1_WAKEUP_RETRIES; ++attempt) {
        sleepCfg = esp_sleep_enable_ext1_wakeup(wakeupMask, ESP_EXT1_WAKEUP_ANY_LOW);
        if (sleepCfg == ESP_OK) break;
        Serial.printf("[Sleep] esp_sleep_enable_ext1_wakeup failed (try %d/%d): %s\n",
                      attempt + 1, EXT1_WAKEUP_RETRIES, esp_err_to_name(sleepCfg));
        delay(5);
    }
    if (sleepCfg != ESP_OK) {
        // 未真正入睡时释放 hold，避免唤醒脚长期卡在 RTC hold 影响按键
        for (gpio_num_t pin : wakePins) {
            if (rtc_gpio_is_valid_gpio(pin)) {
                rtc_gpio_hold_dis(pin);
            }
        }
        return;
    }

    // 不再用“每60秒唤醒”来维持计时：
    // deep sleep 期间由 RTC 计数器自然推进；唤醒时一次性计算经过秒数并补偿。

    // 进入 deep sleep
    esp_deep_sleep_start();
}

