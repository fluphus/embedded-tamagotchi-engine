#include "pet/seriousness.h"
#include <array>
#include <cstdlib>
#include <iostream>

namespace {
constexpr uint32_t T = 1'000'000; // injected calendar epoch seconds
int checks = 0;
#define CHECK(expression) do { ++checks; if (!(expression)) { \
    std::cerr << __func__ << ':' << __LINE__ << " failed: " #expression "\n"; \
    std::exit(1); } } while (false)

PetState fresh(int16_t sr = 50) {
    PetState pet{};
    pet.initNew(T);
    pet.seriousness = sr;
    return pet;
}

void interaction() {
    SeriousnessSystem rules;
    auto pet = fresh();
    pet.idle_minute_remainder = 3;
    const auto result = rules.onInteract(pet, INTERACT_POKE, T);
    CHECK(result.seriousness_before == 50 && result.seriousness_after == 35);
    CHECK(result.tier_before == TIER_MID && result.tier_after == TIER_MID);
    CHECK(!result.tier_changed && result.rhongo_state == RHONGO_INACTIVE);
    CHECK(pet.last_interact_time == T && pet.last_poke_effect_time == T);
    CHECK(pet.idle_paused_until == T + 1800 && pet.idle_minute_remainder == 3);
    CHECK(pet.health == 50 && pet.form == FORM_LILY && pet.age_days == 0);
    const auto feed = rules.onInteract(pet, INTERACT_FEED, T + 1);
    CHECK(pet.seriousness == 20 && feed.tier_changed && feed.tier_after == TIER_LOW);
    CHECK(pet.last_interact_time == T + 1 && pet.last_poke_effect_time == T);
    CHECK(pet.idle_paused_until == T + 1800); // feed does not start/extend pause
}

void cooldown() {
    SeriousnessSystem rules;
    auto pet = fresh(100);
    rules.onInteract(pet, INTERACT_POKE, T);
    for (uint32_t elapsed : {0u, 1u, 1799u}) {
        const auto result = rules.onInteract(pet, INTERACT_POKE, T + elapsed);
        CHECK(result.seriousness_before == 85 && result.seriousness_after == 85);
        CHECK(pet.last_poke_effect_time == T);
        CHECK(pet.last_interact_time == T + elapsed);
        CHECK(pet.idle_paused_until == T + elapsed + 1800);
    }
    rules.onInteract(pet, INTERACT_POKE, T + 1800);
    CHECK(pet.seriousness == 70 && pet.last_poke_effect_time == T + 1800);
    CHECK(pet.idle_paused_until == T + 3600);
    auto after = fresh(100);
    after.last_poke_effect_time = T;
    rules.onInteract(after, INTERACT_POKE, T + 1801);
    CHECK(after.seriousness == 85 && after.last_poke_effect_time == T + 1801);
}

void pauseExpiry() {
    SeriousnessSystem rules;
    auto pet = fresh();
    pet.idle_minute_remainder = 4;
    rules.onInteract(pet, INTERACT_POKE, T);
    rules.onIdleTick(pet, T + 1799);
    CHECK(pet.seriousness == 35 && pet.idle_minute_remainder == 4);
    const auto expiry = rules.onIdleTick(pet, T + 1800);
    CHECK(expiry.seriousness_before == 35 && expiry.seriousness_after == 36);
    CHECK(pet.idle_minute_remainder == 0);
    rules.onIdleTick(pet, T + 1801); // each call supplies one minute, not elapsed detection
    CHECK(pet.seriousness == 36 && pet.idle_minute_remainder == 1);
}

void longAdvance() {
    SeriousnessSystem rules;
    auto pet = fresh();
    rules.onInteract(pet, INTERACT_POKE, T);
    auto stepped = pet;
    const auto result = rules.onIdleBatch(pet, 60, T + 3600);
    for (uint32_t minute = 1; minute <= 60; ++minute)
        rules.onIdleTick(stepped, T + minute * 60);
    // Endpoints minute 30..60 inclusive: 31 active ticks = +6, remainder 1.
    CHECK(result.seriousness_before == 35 && result.seriousness_after == 41);
    CHECK(pet.idle_minute_remainder == 1);
    CHECK(pet.seriousness == stepped.seriousness);
    CHECK(pet.idle_minute_remainder == stepped.idle_minute_remainder);
    CHECK(pet.last_poke_effect_time == T && pet.idle_paused_until == T + 1800);
    rules.onIdleBatch(pet, 1440, T + 3600 + 86400);
    CHECK(pet.seriousness == 100 && pet.idle_minute_remainder == 1);
    CHECK(!pet.is_rhongomyniad && pet.rhongo_timer_start == 0); // child
    const auto zero = rules.onIdleBatch(pet, 0, T + 90000);
    CHECK(zero.seriousness_before == zero.seriousness_after);
    CHECK(pet.idle_minute_remainder == 1);
}

void boundsAndTiers() {
    SeriousnessSystem rules;
    for (int16_t value : {0, 1, 14, 15, 16, 99, 100}) {
        auto pet = fresh(value);
        rules.onInteract(pet, INTERACT_POKE, T);
        CHECK(pet.seriousness == (value < 15 ? 0 : value - 15));
        rules.onInteract(pet, INTERACT_FEED, T + 1);
        CHECK(pet.seriousness >= 0 && pet.seriousness <= 100);
    }
    auto pet = fresh(99);
    rules.onIdleBatch(pet, 10, T + 600);
    CHECK(pet.seriousness == 100 && pet.idle_minute_remainder == 0);
    rules.applyMissedFeedPenalty(pet);
    CHECK(pet.seriousness == 100);
    pet.seriousness = 95;
    rules.applyMissedFeedPenalty(pet);
    CHECK(pet.seriousness == 100);
    pet.seriousness = 0;
    rules.applyMissedFeedPenalty(pet);
    CHECK(pet.seriousness == 10);
    CHECK(rules.getTier(0) == TIER_LOW && rules.getTier(32) == TIER_LOW);
    CHECK(rules.getTier(33) == TIER_MID && rules.getTier(65) == TIER_MID);
    CHECK(rules.getTier(66) == TIER_HIGH && rules.getTier(99) == TIER_HIGH);
    CHECK(rules.getTier(100) == TIER_MAX);
}

std::array<SeriousnessLogEvent, 8> logs{};
size_t logCount = 0;
void capture(const SeriousnessLogEvent& event) {
    CHECK(logCount < logs.size());
    logs[logCount++] = event;
}

void rhongoAndDiagnostics() {
    logCount = 0;
    SeriousnessSystem rules(capture);
    auto pet = fresh(100);
    pet.alignment = ALIGN_WHITE;
    pet.stage = STAGE_ADULT;
    auto start = rules.onIdleTick(pet, T);
    CHECK(pet.rhongo_timer_start == T && start.rhongo_state == RHONGO_COUNTING);
    CHECK(start.rhongo_elapsed_sec == 0 && start.rhongo_remaining_sec == 172800);
    CHECK(logCount == 1 && logs[0].kind == SeriousnessLogKind::RhongoStarted);
    rules.onIdleTick(pet, T + 172799);
    CHECK(!pet.is_rhongomyniad && logCount == 1);
    auto end = rules.onIdleTick(pet, T + 172800);
    CHECK(pet.is_rhongomyniad && pet.form == FORM_WHITE_LANCER_RHONGOMYNIAD);
    CHECK(end.rhongo_state == RHONGO_TRIGGERED && end.rhongo_remaining_sec == 0);
    CHECK(logCount == 2 && logs[1].kind == SeriousnessLogKind::RhongoTriggered);
    rules.onIdleTick(pet, T + 172801);
    CHECK(logCount == 2); // no repeated trigger

    pet = fresh(95);
    pet.alignment = ALIGN_WHITE;
    pet.stage = STAGE_ADULT;
    pet.rhongo_timer_start = T;
    rules.onInteract(pet, INTERACT_FEED, T + 1);
    CHECK(pet.seriousness == 80 && pet.rhongo_timer_start == T);
    pet.seriousness = 94;
    rules.onInteract(pet, INTERACT_FEED, T + 2);
    CHECK(pet.seriousness == 79 && pet.rhongo_timer_start == 0);
    CHECK(logCount == 3 && logs[2].kind == SeriousnessLogKind::RhongoReset);
    CHECK(logs[2].after == 79);
    rules.applyMissedFeedPenalty(pet);
    CHECK(logCount == 4 && logs[3].kind == SeriousnessLogKind::MissedFeedPenalty);
    CHECK(logs[3].before == 79 && logs[3].after == 89);
}

void terminalAndLegacyEdges() {
    SeriousnessSystem rules;
    for (bool white : {false, true}) {
        auto pet = fresh(50);
        pet.is_rhongomyniad = white;
        pet.is_black_rhongomyniad = !white;
        const auto interact = rules.onInteract(pet, INTERACT_POKE, T + 1);
        CHECK(interact.rhongo_state == RHONGO_TRIGGERED && pet.seriousness == 50);
        CHECK(pet.last_interact_time == T && pet.last_poke_effect_time == 0);
        CHECK(pet.idle_paused_until == 0);
        const auto tick = rules.onIdleTick(pet, T + 60);
        const auto batch = rules.onIdleBatch(pet, 1440, T + 86400);
        CHECK(tick.rhongo_state == RHONGO_TRIGGERED && batch.rhongo_state == RHONGO_TRIGGERED);
        CHECK(pet.seriousness == 50 && pet.idle_minute_remainder == 0);
        rules.applyMissedFeedPenalty(pet);
        CHECK(pet.seriousness == (white ? 50 : 60)); // legacy asymmetry, not corrected
    }
    auto zero = fresh();
    rules.onInteract(zero, INTERACT_POKE, 0);
    rules.onInteract(zero, INTERACT_POKE, 1);
    CHECK(zero.seriousness == 20); // 0 means never: epoch-zero poke bypasses cooldown
    auto backwards = fresh();
    backwards.last_poke_effect_time = T;
    rules.onInteract(backwards, INTERACT_POKE, T - 1);
    CHECK(backwards.seriousness == 35); // legacy uint32 subtraction underflows
}
} // namespace

int main() {
    const std::array tests{interaction, cooldown, pauseExpiry, longAdvance,
                          boundsAndTiers, rhongoAndDiagnostics, terminalAndLegacyEdges};
    for (auto test : tests) test();
    std::cout << "PASS: " << tests.size() << " cases, " << checks << " checks\n";
}
