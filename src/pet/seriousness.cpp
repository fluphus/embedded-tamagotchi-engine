// Compatibility facade: all mutation commands execute through the selected engine.
#include "seriousness.h"
#include "../app/runtime.h"
#include "../config/game_config.h"

IdleTickResult SeriousnessSystem::onIdleTick(PetState &pet, uint32_t now) {
    content::logSink = _logSink;
    engine::Command c;
    c.kind = engine::CommandKind::Minute;
    c.time = {now};
    return app::execute(pet, c).idle;
}
IdleTickResult SeriousnessSystem::onIdleBatch(PetState &pet, uint32_t minutes, uint32_t now) {
    content::logSink = _logSink;
    engine::Command c;
    c.kind = engine::CommandKind::AdvanceMinutes;
    c.time = {now};
    c.minutes = minutes;
    return app::execute(pet, c).idle;
}
InteractResult SeriousnessSystem::onInteract(PetState &pet, InteractType type, uint32_t now) {
    content::logSink = _logSink;
    engine::Command c;
    c.kind =
        type == INTERACT_POKE ? engine::CommandKind::Poke : engine::CommandKind::FeedInteraction;
    c.time = {now};
    return app::execute(pet, c).interact;
}
void SeriousnessSystem::applyMissedFeedPenalty(PetState &pet) {
    content::logSink = _logSink;
    engine::Command c;
    c.kind = engine::CommandKind::MissedFeed;
    app::execute(pet, c);
}
SeriousnessTier SeriousnessSystem::getTier(int16_t value) {
    return value >= 100 ? TIER_MAX : value > 65 ? TIER_HIGH : value > 32 ? TIER_MID : TIER_LOW;
}
SeriousnessTier SeriousnessSystem::getCurrentTier(const PetState &pet) {
    return getTier(pet.seriousness);
}
RhongoTimerState SeriousnessSystem::getRhongoState(const PetState &p, uint32_t) {
    return p.is_rhongomyniad                                    ? RHONGO_TRIGGERED
           : p.alignment == ALIGN_WHITE && p.rhongo_timer_start ? RHONGO_COUNTING
                                                                : RHONGO_INACTIVE;
}
uint32_t SeriousnessSystem::getRhongoElapsed(const PetState &p, uint32_t now) {
    return !p.rhongo_timer_start || p.is_rhongomyniad || now <= p.rhongo_timer_start
               ? 0
               : now - p.rhongo_timer_start;
}
uint32_t SeriousnessSystem::getRhongoRemaining(const PetState &p, uint32_t now) {
    const auto elapsed = getRhongoElapsed(p, now);
    return p.is_rhongomyniad || elapsed >= 172800 ? 0 : 172800 - elapsed;
}
