// src/pet/seriousness.h

#ifndef SERIOUSNESS_TYPES_H
#define SERIOUSNESS_TYPES_H

#include <stdint.h>
#include "../artoria/legacy_view.h"

// ============================================
// 严肃值系统
// 管理严肃值的增减、区间判定、狮子王计时
// ============================================

// 严肃值区间
enum SeriousnessTier : uint8_t {
    TIER_LOW = 0,       // 0-32:   娱乐态
    TIER_MID,           // 33-65:  基础态
    TIER_HIGH,          // 66-99:  lancer态
    TIER_MAX            // 100:    狮子王候选
};

inline const char* TIER_NAMES[] = {
    "LOW (0-32, fun)",
    "MID (33-65, base)",
    "HIGH (66-99, lancer)",
    "MAX (100, rhongo candidate)"
};

// 互动类型
enum InteractType : uint8_t {
    INTERACT_FEED = 0,
    INTERACT_POKE
};

// 狮子王计时状态
enum RhongoTimerState : uint8_t {
    RHONGO_INACTIVE = 0,    // 未触发 (严肃值 < 100)
    RHONGO_COUNTING,        // 计时中 (严肃值 == 100, 未满48h)
    RHONGO_TRIGGERED,       // 已触发 (48h已满, 不可逆)
    RHONGO_RESET            // 本次 tick 中计时被重置 (降到80以下)
};

// 待机 tick 结果
struct IdleTickResult {
    int16_t seriousness_before;
    int16_t seriousness_after;
    SeriousnessTier tier_before;
    SeriousnessTier tier_after;
    bool tier_changed;
    RhongoTimerState rhongo_state;
    uint32_t rhongo_elapsed_sec;        // 狮子王计时已过秒数
    uint32_t rhongo_remaining_sec;      // 狮子王计时剩余秒数
};

// 互动结果
struct InteractResult {
    int16_t seriousness_before;
    int16_t seriousness_after;
    SeriousnessTier tier_before;
    SeriousnessTier tier_after;
    bool tier_changed;
    RhongoTimerState rhongo_state;
};

// Diagnostic observations only; sinks must not mutate pet state or re-enter rules.
enum class SeriousnessLogKind : uint8_t {
    RhongoStarted,
    RhongoTriggered,
    RhongoReset,
    MissedFeedPenalty
};

struct SeriousnessLogEvent {
    SeriousnessLogKind kind;
    int16_t before;
    int16_t after;
};

using SeriousnessLogSink = void (*)(const SeriousnessLogEvent&);


#endif
