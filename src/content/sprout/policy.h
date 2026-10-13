#pragma once
#include "../../engine/bytes.h"
#include "../../engine/physiology.h"
#include "../definition.h"
#include "../result.h"
#include "config.h"
namespace sprout {
// Independent content state: accumulated water, plus this package's daily feed record.
struct WaterState {
    uint16_t water = 0;
    uint8_t day = 0, count = 0;
    uint32_t lastFeed = 0;
};
inline WaterState read(const engine::State &s) {
    engine::Reader r{s.extension.data(), s.extension_size};
    WaterState w;
    w.water = r.u16();
    w.day = r.u8();
    w.count = r.u8();
    w.lastFeed = r.u32();
    return w;
}
inline void write(engine::State &s, const WaterState &w) {
    engine::Writer b{s.extension.data(), s.extension.size()};
    b.u16(w.water);
    b.u8(w.day);
    b.u8(w.count);
    b.u32(w.lastFeed);
    s.extension_size = uint8_t(b.size);
    s.extension_version = 1;
}
inline engine::State capture(const PetState &p) {
    engine::State s;
    s.form = p.form;
    s.health = p.health;
    s.seriousness = p.seriousness;
    s.stage = p.stage;
    s.age_days = p.age_days;
    s.birth_timestamp = p.birth_timestamp;
    s.last_interact_time = p.last_interact_time;
    s.idle_minute_remainder = p.idle_minute_remainder;
    s.idle_paused_until = p.idle_paused_until;
    s.last_poke_effect_time = p.last_poke_effect_time;
    s.extension = p.content_extension;
    s.extension_size = p.content_extension_size;
    auto w = read(s);
    w.day = p.daily_feed.date_day;
    w.count = p.daily_feed.feed_count;
    w.lastFeed = p.daily_feed.last_feed_time;
    write(s, w);
    return s;
}
inline PetState project(const engine::State &s) {
    PetState p{};
    p.initNew(s.birth_timestamp);
    p.form = static_cast<Form>(s.form);
    p.base_form = p.form;
    p.white_fun_form = p.form;
    p.health = s.health;
    p.seriousness = s.seriousness;
    p.stage = static_cast<LifeStage>(s.stage);
    p.age_days = s.age_days;
    p.last_interact_time = s.last_interact_time;
    p.idle_minute_remainder = s.idle_minute_remainder;
    p.idle_paused_until = s.idle_paused_until;
    p.last_poke_effect_time = s.last_poke_effect_time;
    p.content_extension = s.extension;
    p.content_extension_size = s.extension_size;
    const auto w = read(s);
    p.daily_feed.date_day = w.day;
    p.daily_feed.feed_count = w.count;
    p.daily_feed.last_feed_time = w.lastFeed;
    return p;
}
inline bool validate(const engine::State &s) {
    if (s.extension_version != 1 || s.extension_size != 8 || !content::knownForm(s.form) ||
        s.stage > 1 || s.health < 0 || s.health > 100 || s.seriousness < 0 || s.seriousness > 100)
        return false;
    const auto w = read(s);
    return w.count <= 3 && w.day <= 31;
}
struct Policy {
    using Result = content::Result;
    static engine::Id resetOption(const engine::State &, uint16_t, const engine::Ports &) {
        return 0;
    }
    static bool terminal(const engine::State &) { return false; }
    static engine::InteractionRules rules() {
        return {HEALTH_MIN,
                HEALTH_MAX,
                SERIOUSNESS_INTERACT_DELTA,
                POKE_SERIOUSNESS_DELTA,
                SERIOUSNESS_IDLE_PER_TICK,
                SERIOUSNESS_IDLE_INTERVAL_MIN,
                POKE_COOLDOWN_SEC,
                POKE_IDLE_PAUSE_SEC};
    }
    static SeriousnessTier tier(int value) {
        return value >= 100 ? TIER_MAX : value > 65 ? TIER_HIGH : value > 32 ? TIER_MID : TIER_LOW;
    }
    static Result handle(engine::State &s, const engine::Command &c, const engine::Ports &) {
        Result r;
        const auto before = s;
        auto w = read(s);
        using K = engine::CommandKind;
        switch (c.kind) {
        case K::Initialize:
        case K::Reset:
            s = {};
            s.form = 40;
            s.birth_timestamp = c.time.value;
            s.last_interact_time = c.time.value;
            w = {};
            break;
        case K::Poke:
        case K::FeedInteraction:
            engine::interact(s, c.kind == K::Poke, c.time.value, rules());
            break;
        case K::Minute:
            engine::idleMinute(s, c.time.value, rules());
            break;
        case K::Feed: {
            r.feed.health_before = s.health;
            r.feed.seriousness_before = s.seriousness;
            if (w.count >= 3) {
                r.feed.result = FEED_ERR_DAILY_LIMIT;
                break;
            }
            if (w.lastFeed && uint32_t(c.time.value - w.lastFeed) < 1800) {
                r.feed.result = FEED_ERR_TOO_SOON;
                r.feed.wait_seconds = 1800 - uint32_t(c.time.value - w.lastFeed);
                break;
            }
            bool valid = true;
            int hp = 0, water = 0;
            bool all = true;
            for (size_t i = 0; i < c.foods.size(); ++i) {
                int index = -1;
                for (int j = 0; j < FOOD_COUNT; ++j)
                    if (FOOD_TABLE[j].id == c.foods[i])
                        index = j;
                if (index < 0) {
                    valid = false;
                    break;
                }
                r.feed.picked_ids[i] = uint8_t(index);
                hp += FOOD_TABLE[index].health_delta;
                water += c.foods[i] == FOOD_DEW         ? 2
                         : c.foods[i] == FOOD_SUNSEED   ? 3
                         : c.foods[i] == FOOD_MOONBERRY ? 1
                                                        : 0;
                all = all && FOOD_TABLE[index].is_healthy;
            }
            if (!valid) {
                r.feed.result = FEED_ERR_INVALID_FOOD;
                break;
            }
            s.health = engine::bounded(s.health + hp);
            w.water = uint16_t(uint32_t(w.water)+water>UINT16_MAX?UINT16_MAX:w.water+water);
            ++w.count;
            w.lastFeed = c.time.value;
            s.last_interact_time = c.time.value;
            r.feed.health_from_food = hp;
            r.feed.health_after = s.health;
            r.feed.seriousness_after = s.seriousness;
            r.feed.combo_triggered = all;
            r.feed.combo = all ? COMBO_ALL_HEALTHY : COMBO_NONE;
            r.feed.special_food_id = -1;
            r.feed.result = FEED_OK;
            break;
        }
        case K::SpecialFood:
            if (c.option == 101 || c.option == 107) {
                s.health = engine::bounded(s.health + 5);
                if(w.water<UINT16_MAX)++w.water;
                r.feed.combo_triggered = true;
                r.feed.special_food_id = c.option == 101 ? 0 : 1;
            }
            break;
        case K::GrowthCheck:
        case K::EvolutionCheck:
            r.evolution.form_before = static_cast<Form>(s.form);
            if (s.stage == 0 && s.age_days >= 1 && w.water >= 6) {
                s.stage = 1;
                s.form = 47;
                r.evolution.event = EVO_FORM_CHANGED;
            }
            r.evolution.form_after = static_cast<Form>(s.form);
            r.evolution.tier = tier(s.seriousness);
            break;
        case K::DayEffects:
            r.day.health_before = s.health;
            r.day.health_after = s.health;
            r.day.seriousness_before = s.seriousness;
            r.day.seriousness_after = s.seriousness;
            break;
        case K::MissedFeed:
            break; // this package has no missed-feed punishment
        default:
            break;
        }
        write(s, w);
        r.interact.seriousness_before = before.seriousness;
        r.interact.seriousness_after = s.seriousness;
        r.interact.tier_before = tier(before.seriousness);
        r.interact.tier_after = tier(s.seriousness);
        r.interact.tier_changed = r.interact.tier_before != r.interact.tier_after;
        summarize(before, s, c, r);
        return r;
    }
    static void summarize(const engine::State &before, const engine::State &after,
                          const engine::Command &, Result &r) {
        r.idle.seriousness_before = before.seriousness;
        r.idle.seriousness_after = after.seriousness;
        r.idle.tier_before = tier(before.seriousness);
        r.idle.tier_after = tier(after.seriousness);
        r.idle.tier_changed = r.idle.tier_before != r.idle.tier_after;
    }
};
} // namespace sprout
