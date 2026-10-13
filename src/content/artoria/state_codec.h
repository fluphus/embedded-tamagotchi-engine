#pragma once
#include "../../engine/bytes.h"
#include "../../engine/contract.h"
#include "legacy_view.h"
namespace artoria {
// Version 1 extension owns all alignment, food-window and special-route state.
inline engine::State capture(const PetState &p) {
    engine::State s;
    s.form = p.form;
    s.health = p.health;
    s.seriousness = p.seriousness;
    s.idle_minute_remainder = p.idle_minute_remainder;
    s.idle_paused_until = p.idle_paused_until;
    s.last_poke_effect_time = p.last_poke_effect_time;
    s.birth_timestamp = p.birth_timestamp;
    s.last_interact_time = p.last_interact_time;
    s.age_days = p.age_days;
    s.stage = p.stage;
    engine::Writer w{s.extension.data(), s.extension.size()};
    w.u8(p.base_form);
    w.u8(p.alignment);
    w.u32(p.rhongo_timer_start);
    w.u8(p.is_rhongomyniad);
    w.u8(p.white_fun_form);
    w.u8(p.white_fun_form_locked);
    w.u8(p.mapo_tofu_count);
    w.u8(p.is_black_rhongomyniad);
    w.u8(p.is_nobu);
    w.u8(p.is_oda_nobunaga);
    w.u8(p.daily_feed.date_day);
    w.u8(p.daily_feed.feed_count);
    w.u32(p.daily_feed.last_feed_time);
    w.u8(p.daily_feed.healthy_in_window);
    w.u8(p.daily_feed.junk_in_window);
    w.u8(p.daily_feed.healthy_outside_window);
    w.u8(p.daily_feed.junk_outside_window);
    s.extension_size = uint8_t(w.size);
    return s;
}
inline PetState project(const engine::State &s) {
    PetState p{};
    p.form = static_cast<Form>(s.form);
    p.health = s.health;
    p.seriousness = s.seriousness;
    p.idle_minute_remainder = s.idle_minute_remainder;
    p.idle_paused_until = s.idle_paused_until;
    p.last_poke_effect_time = s.last_poke_effect_time;
    p.birth_timestamp = s.birth_timestamp;
    p.last_interact_time = s.last_interact_time;
    p.age_days = s.age_days;
    p.stage = static_cast<LifeStage>(s.stage);
    engine::Reader r{s.extension.data(), s.extension_size};
    p.base_form = static_cast<Form>(r.u8());
    p.alignment = static_cast<Alignment>(r.u8());
    p.rhongo_timer_start = r.u32();
    p.is_rhongomyniad = r.u8();
    p.white_fun_form = static_cast<Form>(r.u8());
    p.white_fun_form_locked = r.u8();
    p.mapo_tofu_count = r.u8();
    p.is_black_rhongomyniad = r.u8();
    p.is_nobu = r.u8();
    p.is_oda_nobunaga = r.u8();
    p.daily_feed.date_day = r.u8();
    p.daily_feed.feed_count = r.u8();
    p.daily_feed.last_feed_time = r.u32();
    p.daily_feed.healthy_in_window = r.u8();
    p.daily_feed.junk_in_window = r.u8();
    p.daily_feed.healthy_outside_window = r.u8();
    p.daily_feed.junk_outside_window = r.u8();
    return p;
}
inline bool validate(const engine::State &s) {
    if (s.extension_version != 1 || s.extension_size != 23 || s.form > 11 || s.stage > 1 ||
        s.health < 0 || s.health > 100 || s.seriousness < 0 || s.seriousness > 100)
        return false;
    const auto p = project(s);
    if (p.base_form > 11 || p.white_fun_form > 11 || p.alignment > ALIGN_BLACK ||
        p.daily_feed.feed_count > 3 || p.daily_feed.date_day > 31)
        return false;
    for (size_t i : {size_t(6), size_t(8), size_t(10), size_t(11), size_t(12)})
        if (s.extension[i] > 1)
            return false;
    return true;
}
} // namespace artoria
