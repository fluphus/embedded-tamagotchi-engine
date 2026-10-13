#if !defined(CONTENT_SPROUT) || !CONTENT_SPROUT
// src/pet/seriousness.cpp

#include "seriousness.h"
#include "config.h"
#include "../../engine/physiology.h"

void ArtoriaSeriousnessSystem::log(SeriousnessLogKind kind, int16_t before, int16_t after) const {
    if (_logSink) _logSink({kind, before, after});
}

// --- 工具 ---

int16_t ArtoriaSeriousnessSystem::clamp(int16_t value) {
    if (value > SERIOUSNESS_MAX) return SERIOUSNESS_MAX;
    if (value < SERIOUSNESS_MIN) return SERIOUSNESS_MIN;
    return value;
}

SeriousnessTier ArtoriaSeriousnessSystem::getTier(int16_t seriousness) {
    if (seriousness >= RHONGOMYNIAD_THRESHOLD) return TIER_MAX;
    if (seriousness > SERIOUSNESS_TIER_MID_UPPER) return TIER_HIGH;
    if (seriousness > SERIOUSNESS_TIER_LOW_UPPER) return TIER_MID;
    return TIER_LOW;
}

SeriousnessTier ArtoriaSeriousnessSystem::getCurrentTier(const PetState& pet) {
    return getTier(pet.seriousness);
}

// --- 狮子王计时 ---

void ArtoriaSeriousnessSystem::updateRhongoTimer(PetState& pet, uint32_t currentTime) {
    if (pet.alignment != ALIGN_WHITE) return;
    if (pet.stage != STAGE_ADULT) return;
    if (pet.is_rhongomyniad) return;

    if (pet.seriousness >= RHONGOMYNIAD_THRESHOLD) {
        if (pet.rhongo_timer_start == 0) {
            pet.rhongo_timer_start = currentTime;
            log(SeriousnessLogKind::RhongoStarted, pet.seriousness, pet.seriousness);
        }

        uint32_t elapsed = currentTime - pet.rhongo_timer_start;
        if (elapsed >= RHONGOMYNIAD_SUSTAIN_SEC) {
            pet.is_rhongomyniad = true;
            pet.form = FORM_WHITE_LANCER_RHONGOMYNIAD;
            log(SeriousnessLogKind::RhongoTriggered, pet.seriousness, pet.seriousness);
        }
    } else if (pet.seriousness < RHONGOMYNIAD_SAFE_DROP) {
        if (pet.rhongo_timer_start > 0) {
            log(SeriousnessLogKind::RhongoReset, pet.seriousness, pet.seriousness);
            pet.rhongo_timer_start = 0;
        }
    }
}

RhongoTimerState ArtoriaSeriousnessSystem::getRhongoState(const PetState& pet, uint32_t currentTime) {
    if (pet.is_rhongomyniad) return RHONGO_TRIGGERED;
    if (pet.alignment != ALIGN_WHITE) return RHONGO_INACTIVE;
    if (pet.rhongo_timer_start == 0) return RHONGO_INACTIVE;
    return RHONGO_COUNTING;
}

uint32_t ArtoriaSeriousnessSystem::getRhongoElapsed(const PetState& pet, uint32_t currentTime) {
    if (pet.rhongo_timer_start == 0 || pet.is_rhongomyniad) return 0;
    if (currentTime > pet.rhongo_timer_start)
        return currentTime - pet.rhongo_timer_start;
    return 0;
}

uint32_t ArtoriaSeriousnessSystem::getRhongoRemaining(const PetState& pet, uint32_t currentTime) {
    if (pet.is_rhongomyniad) return 0;
    if (pet.rhongo_timer_start == 0) return RHONGOMYNIAD_SUSTAIN_SEC;
    uint32_t elapsed = getRhongoElapsed(pet, currentTime);
    if (elapsed >= RHONGOMYNIAD_SUSTAIN_SEC) return 0;
    return RHONGOMYNIAD_SUSTAIN_SEC - elapsed;
}

// --- 待机 tick ---

IdleTickResult ArtoriaSeriousnessSystem::onIdleTick(PetState& pet, uint32_t currentTime) {
    IdleTickResult result = {};
    result.seriousness_before = pet.seriousness;
    result.tier_before = getTier(pet.seriousness);

    if (pet.is_rhongomyniad || pet.is_black_rhongomyniad) {
        result.seriousness_after = pet.seriousness;
        result.tier_after = result.tier_before;
        result.tier_changed = false;
        result.rhongo_state = RHONGO_TRIGGERED;
        return result;
    }

    engine::idleMinute(pet, currentTime, engine::InteractionRules{SERIOUSNESS_MIN,SERIOUSNESS_MAX,SERIOUSNESS_INTERACT_DELTA,POKE_SERIOUSNESS_DELTA,SERIOUSNESS_IDLE_PER_TICK,SERIOUSNESS_IDLE_INTERVAL_MIN,POKE_COOLDOWN_SEC,POKE_IDLE_PAUSE_SEC});

    updateRhongoTimer(pet, currentTime);

    result.seriousness_after = pet.seriousness;
    result.tier_after = getTier(pet.seriousness);
    result.tier_changed = (result.tier_before != result.tier_after);
    result.rhongo_state = getRhongoState(pet, currentTime);
    result.rhongo_elapsed_sec = getRhongoElapsed(pet, currentTime);
    result.rhongo_remaining_sec = getRhongoRemaining(pet, currentTime);

    return result;
}



// --- 互动 ---

InteractResult ArtoriaSeriousnessSystem::onInteract(PetState& pet, InteractType type, uint32_t currentTime) {
    InteractResult result = {};
    result.seriousness_before = pet.seriousness;
    result.tier_before = getTier(pet.seriousness);

    if (pet.is_rhongomyniad || pet.is_black_rhongomyniad) {
        result.seriousness_after = pet.seriousness;
        result.tier_after = result.tier_before;
        result.tier_changed = false;
        result.rhongo_state = RHONGO_TRIGGERED;
        return result;
    }

    engine::interact(pet, type == INTERACT_POKE, currentTime, engine::InteractionRules{SERIOUSNESS_MIN,SERIOUSNESS_MAX,SERIOUSNESS_INTERACT_DELTA,POKE_SERIOUSNESS_DELTA,SERIOUSNESS_IDLE_PER_TICK,SERIOUSNESS_IDLE_INTERVAL_MIN,POKE_COOLDOWN_SEC,POKE_IDLE_PAUSE_SEC});

    updateRhongoTimer(pet, currentTime);

    result.seriousness_after = pet.seriousness;
    result.tier_after = getTier(pet.seriousness);
    result.tier_changed = (result.tier_before != result.tier_after);
    result.rhongo_state = getRhongoState(pet, currentTime);

    return result;
}

// --- 未喂满惩罚 ---

void ArtoriaSeriousnessSystem::applyMissedFeedPenalty(PetState& pet) {
    if (pet.is_rhongomyniad) return;

    int16_t old = pet.seriousness;
    pet.seriousness = clamp(pet.seriousness + MISSED_FEED_SERIOUSNESS);

    log(SeriousnessLogKind::MissedFeedPenalty, old, pet.seriousness);
}

#endif
