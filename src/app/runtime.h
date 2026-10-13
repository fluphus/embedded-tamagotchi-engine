#pragma once
#include "../content/selected.h"
#include "../engine/engine.h"
#include "../pet/evolution.h"
#include "../pet/feeding.h"
#include "../pet/seriousness.h"
#include "diagnostics.h"
namespace app {
inline engine::Engine<selected::Policy> runtime;
inline content::Result execute(PetState &view, engine::Command command) {
    auto state = selected::capture(view);
    auto result = runtime.execute(state, command);
    view = selected::project(state);
    return result;
}
inline void resetPet(PetState &view, uint32_t now, uint16_t rounds) {
    engine::Command command;
    command.kind = engine::CommandKind::Reset;
    command.time = {now};
    command.option =
        selected::Policy::resetOption(selected::capture(view), rounds, runtime.ports());
    execute(view, command);
}
inline void configure(engine::Ports ports) {
    app::ports = ports;
    runtime.setPorts(ports);
}
} // namespace app
