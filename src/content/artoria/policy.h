#pragma once
#include "../../engine/diagnostics.h"
#include "../result.h"
#include "evolution.h"
#include "feeding.h"
#include "seriousness.h"
#include "state_codec.h"
namespace artoria {
struct Policy {
    using Result = content::Result;
    static engine::Id resetOption(const engine::State &s, uint16_t rounds,
                                  const engine::Ports &ports) {
        if (rounds == 329)
            return 1;
        const uint32_t roll = ports.random ? ports.random(ports.context) % 1000 : 0;
        return roll < (s.age_days == 5 ? 250u : 10u) ? 1 : 0;
    }
    static bool terminal(const engine::State &s) {
        const auto p = project(s);
        return p.is_rhongomyniad || p.is_black_rhongomyniad;
    }
    static Result handle(engine::State &state, const engine::Command &c,
                         const engine::Ports &ports) {
        engine::support::ports = ports;
        PetState p = project(state);
        Result r;
        ArtoriaFeedingSystem feeding;
        ArtoriaEvolutionSystem evolution;
        ArtoriaSeriousnessSystem seriousness(content::logSink);
        const auto now = c.time.value;
        using K = engine::CommandKind;
        switch (c.kind) {
        case K::Initialize:
            p.initNew(now);
            break;
        case K::Poke:
            r.interact = seriousness.onInteract(p, INTERACT_POKE, now);
            break;
        case K::FeedInteraction:
            r.interact = seriousness.onInteract(p, INTERACT_FEED, now);
            break;
        case K::Minute:
            r.idle = seriousness.onIdleTick(p, now);
            break;
        case K::Feed: {
            uint8_t picked[3] = {255, 255, 255};
            for (size_t i = 0; i < 3; ++i)
                for (uint8_t index = 0; index < FOOD_COUNT; ++index)
                    if (FOOD_TABLE[index].id == c.foods[i])
                        picked[i] = index;
            r.feed = feeding.feed(p, picked, now, c.hour);
            break;
        }
        case K::SpecialFood:
            for (uint8_t index = 0; index < SFOOD_COUNT; ++index) {
                if (SPECIAL_FOOD_TABLE[index].id != c.option)
                    continue;
                r.feed.combo_triggered = true;
                r.feed.mapo_tofu_total = p.mapo_tofu_count;
                feeding.applySpecialFood(p, r.feed, index);
                break;
            }
            break;
        case K::DayEffects:
            r.day = feeding.processDayEnd(p);
            break;
        case K::MissedFeed:
            seriousness.applyMissedFeedPenalty(p);
            break;
        case K::GrowthCheck:
            r.evolution = evolution.checkChildGraduation(p);
            break;
        case K::EvolutionCheck:
            r.evolution = evolution.check(p, now);
            break;
        case K::Reset:
            if (c.option == 1)
                evolution.destroyToNobu(p, now, p.age_days);
            else
                evolution.destroy(p, now);
            break;
        case K::ContentAction:
            if (c.option == 1)
                r.evolution = evolution.checkMapoCurse(p);
            else if (c.option == 2) {
                r.evolution.form_before = p.form;
                r.evolution.form_after = evolution.resolveAdultForm(p);
            } else
                r.evolution = evolution.checkNobuMapo(p);
            break;
        default:
            break;
        }
        state = capture(p);
        return r;
    }
    static void summarize(const engine::State &before, const engine::State &after,
                          const engine::Command &c, Result &r) {
        ArtoriaSeriousnessSystem rules;
        const auto p = project(after);
        r.idle.seriousness_before = before.seriousness;
        r.idle.seriousness_after = after.seriousness;
        r.idle.tier_before = rules.getTier(before.seriousness);
        r.idle.tier_after = rules.getTier(after.seriousness);
        r.idle.tier_changed = r.idle.tier_before != r.idle.tier_after;
        r.idle.rhongo_state = rules.getRhongoState(p, c.time.value);
        r.idle.rhongo_elapsed_sec = rules.getRhongoElapsed(p, c.time.value);
        r.idle.rhongo_remaining_sec = rules.getRhongoRemaining(p, c.time.value);
        if (terminal(before)) {
            r.idle.rhongo_state = RHONGO_TRIGGERED;
            r.idle.rhongo_elapsed_sec = r.idle.rhongo_remaining_sec = 0;
        }
    }
};
} // namespace artoria
