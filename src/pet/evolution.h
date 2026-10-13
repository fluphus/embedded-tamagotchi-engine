#pragma once
#include "../content/contracts/evolution_types.h"
class EvolutionSystem {
public:
    EvolutionResult check(PetState& pet, uint32_t currentTime);
    EvolutionResult checkChildGraduation(PetState& pet);
    EvolutionResult checkMapoCurse(PetState& pet);     // 新增
    EvolutionResult checkNobuMapo(PetState& pet);
    void destroy(PetState& pet, uint32_t currentTime);
    void destroyToNobu(PetState& pet, uint32_t currentTime, uint16_t prevAgeDays);
    Form resolveAdultForm(PetState& pet);
    bool canInteract(const PetState& pet);

};

extern EvolutionSystem evolutionSystem;
