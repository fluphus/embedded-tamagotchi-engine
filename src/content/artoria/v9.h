#pragma once
#include "../../engine/save_codec.h"
#include "state_codec.h"
namespace artoria {
// Offsets are from tests/fixtures/v9/manifest.json, emitted by ESP32-S3 compiler.
inline engine::DecodeError migrateV9(const uint8_t *data, size_t size, engine::Document &output) {
    using E = engine::DecodeError;
    if (size != 72 && size != 84)
        return E::Length;
    engine::Reader h{data, 16};
    if (h.u8() != 9)
        return E::Version;
    h.u8();
    if (h.u16() != 56)
        return E::Length;
    const auto crc = h.u16();
    h.u16();
    const auto sequence = h.u32(), savedAt = h.u32();
    if (engine::checksum(data + 16, 56) != crc)
        return E::Checksum;
    const uint8_t *b = data + 16;
    auto u16 = [&](size_t i) {
        engine::Reader r{b + i, 56 - i};
        return r.u16();
    };
    auto u32 = [&](size_t i) {
        engine::Reader r{b + i, 56 - i};
        return r.u32();
    };
    for (size_t i : {size_t(24), size_t(26), size_t(28), size_t(29), size_t(30)})
        if (b[i] > 1)
            return E::Value;
    PetState p{};
    p.form = static_cast<Form>(b[0]);
    p.base_form = static_cast<Form>(b[1]);
    p.alignment = static_cast<Alignment>(b[2]);
    p.stage = static_cast<LifeStage>(b[3]);
    p.health = int16_t(u16(4));
    p.seriousness = int16_t(u16(6));
    p.idle_minute_remainder = u16(8);
    p.idle_paused_until = u32(12);
    p.last_poke_effect_time = u32(16);
    p.rhongo_timer_start = u32(20);
    p.is_rhongomyniad = b[24];
    p.white_fun_form = static_cast<Form>(b[25]);
    p.white_fun_form_locked = b[26];
    p.mapo_tofu_count = b[27];
    p.is_black_rhongomyniad = b[28];
    p.is_nobu = b[29];
    p.is_oda_nobunaga = b[30];
    p.birth_timestamp = u32(32);
    p.last_interact_time = u32(36);
    p.age_days = u16(40);
    p.daily_feed.date_day = b[44];
    p.daily_feed.feed_count = b[45];
    p.daily_feed.last_feed_time = u32(48);
    p.daily_feed.healthy_in_window = b[52];
    p.daily_feed.junk_in_window = b[53];
    p.daily_feed.healthy_outside_window = b[54];
    p.daily_feed.junk_outside_window = b[55];
    engine::Document d;
    d.state = capture(p);
    d.sequence = sequence;
    d.savedAt = savedAt;
    d.full = size == 84;
    if (!validate(d.state))
        return E::Value;
    if (d.full)
        for (uint8_t i = 0; i < 12; ++i) {
            if (data[72 + i] > 1)
                return E::Value;
            if (data[72 + i])
                d.gallery.unlock(i);
        }
    output = d;
    return E::Ok;
}
} // namespace artoria
