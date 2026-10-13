#include "evolution.h"
#include "../app/runtime.h"
EvolutionSystem evolutionSystem;
EvolutionResult EvolutionSystem::check(PetState &p, uint32_t now) {
    engine::Command c;
    c.kind = engine::CommandKind::EvolutionCheck;
    c.time = {now};
    return app::execute(p, c).evolution;
}
EvolutionResult EvolutionSystem::checkChildGraduation(PetState &p) {
    engine::Command c;
    c.kind = engine::CommandKind::GrowthCheck;
    return app::execute(p, c).evolution;
}
EvolutionResult EvolutionSystem::checkMapoCurse(PetState &p) {
    engine::Command c;
    c.kind = engine::CommandKind::ContentAction;
    c.option = 1;
    return app::execute(p, c).evolution;
}
EvolutionResult EvolutionSystem::checkNobuMapo(PetState &p) {
    engine::Command c;
    c.kind = engine::CommandKind::ContentAction;
    return app::execute(p, c).evolution;
}
void EvolutionSystem::destroy(PetState &p, uint32_t now) {
    engine::Command c;
    c.kind = engine::CommandKind::Reset;
    c.time = {now};
    app::execute(p, c);
}
void EvolutionSystem::destroyToNobu(PetState &p, uint32_t now, uint16_t) {
    engine::Command c;
    c.kind = engine::CommandKind::Reset;
    c.time = {now};
    c.option = 1;
    app::execute(p, c);
}
Form EvolutionSystem::resolveAdultForm(PetState &p) {
    engine::Command c;
    c.kind = engine::CommandKind::ContentAction;
    c.option = 2;
    return app::execute(p, c).evolution.form_after;
}
bool EvolutionSystem::canInteract(const PetState &p) {
    return !selected::Policy::terminal(selected::capture(p));
}
