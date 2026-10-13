#pragma once
#include "../core/game_state.h"
#include "diagnostics.h"
inline void realignOwnerTimestamps(PetState& ownerPet, uint32_t frozenDuration) {
    // 所有绝对时间戳 += frozenDuration, 使主人宠物的时间锚点对齐到当前设备时间
    if (ownerPet.idle_paused_until > 0)
        ownerPet.idle_paused_until += frozenDuration;
    if (ownerPet.last_poke_effect_time > 0)
        ownerPet.last_poke_effect_time += frozenDuration;
    if (ownerPet.rhongo_timer_start > 0)
        ownerPet.rhongo_timer_start += frozenDuration;
    if (ownerPet.birth_timestamp > 0)
        ownerPet.birth_timestamp += frozenDuration;
    if (ownerPet.last_interact_time > 0)
        ownerPet.last_interact_time += frozenDuration;
    if (ownerPet.daily_feed.last_feed_time > 0)
        ownerPet.daily_feed.last_feed_time += frozenDuration;
}

// 导入宠物时: 将访客/备份宠物的绝对时间戳从导出方时钟对齐到本机时钟
// delta = localNow - donorSaveTime (有符号, 支持本机时钟在前或在后)
// 原理: 导出方存档时 save_time 是其设备时钟, 宠物内所有绝对时间戳也基于该时钟;
//        导入方用 (本机now - 导出方save_time) 作为偏移量, 将宠物时间锚定到本机时钟.
// 安全护栏: |delta| > 10年 视为数据异常, 跳过对齐 (避免溢出/乱标定)
inline void realignImportedTimestamps(PetState& pet, uint32_t donorSaveTime, uint32_t localNow, uint8_t localDay) {
    if (donorSaveTime == 0) {         // 无锚点, 无法对齐; 仍重置日界
        pet.daily_feed.reset(localDay);
        return;
    }

    int32_t delta = (int32_t)(localNow - donorSaveTime);

    // 安全护栏: |delta| > 10年 (~315M秒) 视为异常, 不做平移
    constexpr int32_t MAX_REASONABLE_DELTA = 315360000;  // 10 * 365 * 24 * 3600
    if (delta > MAX_REASONABLE_DELTA || delta < -MAX_REASONABLE_DELTA) {
        app::tracef("[TimeAlign] WARNING: delta=%ld sec (>10yr), skipping realignment\n", (long)delta);
        // 即使跳过平移, 仍重置日界 (否则 daily_feed 必然错乱)
        pet.daily_feed.reset(localDay);
        return;
    }

    // 若 delta 很小 (< 60秒), 不值得平移, 避免无意义微调
    if (delta > -60 && delta < 60) {
        // 仍对齐日界
        pet.daily_feed.reset(localDay);
        return;
    }

    app::tracef("[TimeAlign] Realigning imported pet timestamps, delta=%ld sec\n", (long)delta);

    // 辅助 lambda: 对非零时间戳施加有符号偏移, 并 clamp 到 [1, localNow] 防止溢出/未来值
    auto shiftTimestamp = [delta, localNow](uint32_t& ts) {
        if (ts == 0) return;  // 0 = 未设置/特殊语义, 保留
        int64_t shifted = (int64_t)ts + delta;
        if (shifted <= 0) shifted = 1;                    // 不允许变为 0 (0 有特殊含义)
        if (shifted > (int64_t)localNow) shifted = localNow;  // 不允许超过当前时间
        ts = (uint32_t)shifted;
    };

    shiftTimestamp(pet.idle_paused_until);
    shiftTimestamp(pet.last_poke_effect_time);
    shiftTimestamp(pet.rhongo_timer_start);
    shiftTimestamp(pet.birth_timestamp);
    shiftTimestamp(pet.last_interact_time);
    shiftTimestamp(pet.daily_feed.last_feed_time);

    // 日界强制对齐到本机当天 (date_day 是日历日, 平移无意义, 直接 reset)
    pet.daily_feed.reset(localDay);
}


namespace app {
inline uint32_t frozenDuration(uint32_t started,uint32_t now){return started&&now>=started?now-started:0;}
inline constexpr uint32_t MaxOfflineSeconds=366u*86400u;
inline bool offlineSupported(uint32_t seconds,uint16_t age,uint32_t epoch){
    return seconds<=MaxOfflineSeconds&&seconds/86400u<=uint32_t(UINT16_MAX-age)&&seconds<=UINT32_MAX-epoch;
}
struct OfflinePlan {uint32_t days,minutes;};
inline OfflinePlan offlinePlan(uint32_t seconds){return {seconds/86400,(seconds%86400)/60};}
}
