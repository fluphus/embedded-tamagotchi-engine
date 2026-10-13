// src/pet/feeding.h

#ifndef FEEDING_TYPES_H
#define FEEDING_TYPES_H

#include <stdint.h>
#include "../artoria/legacy_view.h"
#include "../../config/food_table.h"

enum FeedResult : uint8_t {
    FEED_OK = 0,
    FEED_ERR_RHONGOMYNIAD,
    FEED_ERR_DAILY_LIMIT,
    FEED_ERR_TOO_SOON,
    FEED_ERR_INVALID_FOOD
};

inline const char* FEED_RESULT_NAMES[] = {
    "OK",
    "Rhongomyniad (no interaction)",
    "Daily limit reached",
    "Too soon (wait 30min)",
    "Invalid food ID"
};

struct FeedDraw {
    uint8_t food_ids[4];
};

enum ComboType : uint8_t {
    COMBO_NONE = 0,
    COMBO_ALL_HEALTHY,
    COMBO_ALL_JUNK
};

inline const char* COMBO_NAMES[] = {
    "None",
    "ALL HEALTHY",
    "ALL JUNK"
};

struct FeedOutcome {
    FeedResult result;

    uint8_t picked_ids[3];

    int16_t health_before;
    int16_t health_after;
    int16_t health_from_food;
    int16_t health_from_combo;

    int16_t seriousness_before;
    int16_t seriousness_after;
    int16_t seriousness_from_dislike;
    int16_t seriousness_from_combo;

    ComboType combo;
    bool combo_triggered;
    int8_t special_food_id;

    // 麻婆豆腐彩蛋
    bool mapo_tofu_triggered;           // 本次是否触发
    uint8_t mapo_tofu_total;            // 触发后的累计次数
    bool mapo_tofu_curse_activated;     // 是否达到诅咒阈值

    bool in_correct_window;
    uint32_t wait_seconds;
};

struct DayEndOutcome {
    bool window_bonus_applied;
    bool window_penalty_applied;
    bool missed_feed_penalty;
    int16_t health_before;
    int16_t health_after;
    int16_t seriousness_before;
    int16_t seriousness_after;
};


#endif
