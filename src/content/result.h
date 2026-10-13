#pragma once
// Typed application-facing outcomes. Legacy field names are presentation compatibility,
// not part of the engine state or wire format.
#include "contracts/feeding_types.h"
#include "contracts/evolution_types.h"
namespace content {
inline SeriousnessLogSink logSink=nullptr;
struct Result {
    FeedOutcome feed{};
    IdleTickResult idle{};
    InteractResult interact{};
    EvolutionResult evolution{};
    DayEndOutcome day{};
};
}
