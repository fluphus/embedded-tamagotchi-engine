#pragma once
#include <cstdint>
namespace engine {
struct InteractionRules {
    int16_t minimum = 0, maximum = 100;
    int16_t feedDelta = 15, pokeDelta = 15, idleDelta = 1;
    uint16_t idleInterval = 5;
    uint32_t cooldown = 1800, pause = 1800;
};
inline int16_t bounded(int value, int16_t minimum = 0, int16_t maximum = 100) {
    return static_cast<int16_t>(value < minimum ? minimum : (value > maximum ? maximum : value));
}
template <class State>
void idleMinute(State &state, uint32_t epoch, const InteractionRules &rules) {
    if (state.idle_paused_until && epoch < state.idle_paused_until)
        return;
    if (++state.idle_minute_remainder >= rules.idleInterval) {
        state.seriousness =
            bounded(state.seriousness + rules.idleDelta, rules.minimum, rules.maximum);
        state.idle_minute_remainder = 0;
    }
}
template <class State>
void interact(State &state, bool poke, uint32_t epoch, const InteractionRules &rules) {
    state.last_interact_time = epoch;
    if (!poke) {
        state.seriousness =
            bounded(state.seriousness - rules.feedDelta, rules.minimum, rules.maximum);
        return;
    }
    state.idle_paused_until = epoch + rules.pause;
    if (!state.last_poke_effect_time ||
        uint32_t(epoch - state.last_poke_effect_time) >= rules.cooldown) {
        state.seriousness =
            bounded(state.seriousness - rules.pokeDelta, rules.minimum, rules.maximum);
        state.last_poke_effect_time = epoch;
    }
}
} // namespace engine
