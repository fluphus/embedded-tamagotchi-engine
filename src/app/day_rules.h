#pragma once
#include "runtime.h"
namespace app {
inline void initializePet(PetState &pet, uint32_t epoch) {
    engine::Command c;
    c.kind = engine::CommandKind::Initialize;
    c.time = {epoch};
    execute(pet, c);
}
inline IdleTickResult startDay(PetState &pet, uint32_t now) {
    pet.idle_paused_until = 0;
    return seriousnessSystem.onIdleBatch(pet, 1440, now);
}
inline EvolutionResult growDay(PetState &pet) {
    if(pet.age_days==UINT16_MAX){trace("[DayEnd] REJECTED: age capacity reached");return {};}
    ++pet.age_days;
    return evolutionSystem.checkChildGraduation(pet);
}
struct DaySummary {
    IdleTickResult idle{};
    DayEndOutcome feeding{};
    EvolutionResult evolution{};
};
inline DaySummary settleDay(PetState &pet, uint32_t now, uint8_t day) {
    DaySummary r;
    if(pet.age_days==UINT16_MAX){trace("[DayEnd] REJECTED: age capacity reached");return r;}
    r.idle = startDay(pet, now);
    if (evolutionSystem.canInteract(pet)) {
        r.feeding = feedingSystem.processDayEnd(pet);
        if (r.feeding.missed_feed_penalty)
            seriousnessSystem.applyMissedFeedPenalty(pet);
        r.evolution = growDay(pet);
        if (r.evolution.event == EVO_NONE && pet.stage == STAGE_ADULT)
            r.evolution = evolutionSystem.check(pet, now);
    } else
        ++pet.age_days;
    feedingSystem.resetDaily(pet, day);
    return r;
}
} // namespace app
