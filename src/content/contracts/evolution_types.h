// src/pet/evolution.h

#ifndef EVOLUTION_TYPES_H
#define EVOLUTION_TYPES_H

#include <stdint.h>
#include "../artoria/legacy_view.h"
#include "seriousness_types.h"

enum EvolutionEvent : uint8_t {
    EVO_NONE = 0,
    EVO_CHILD_TO_WHITE,
    EVO_CHILD_TO_BLACK,
    EVO_FORM_CHANGED,
    EVO_RHONGOMYNIAD,
    EVO_BLACK_RHONGOMYNIAD,         // 新增: 麻婆豆腐诅咒
    EVO_NOBU_EVOLUTION,             // nobu -> Oda Nobunaga
    EVO_DESTROYED
};

inline const char* EVO_EVENT_NAMES[] = {
    "None",
    "Child -> White line",
    "Child -> Black line",
    "Form changed",
    "RHONGOMYNIAD (irreversible)",
    "BLACK RHONGOMYNIAD (curse)",   // 新增
    "nobu -> Oda Nobunaga",
    "Destroyed & Reset"
};

struct EvolutionResult {
    EvolutionEvent event;
    Form form_before;
    Form form_after;
    SeriousnessTier tier;
};


#endif
