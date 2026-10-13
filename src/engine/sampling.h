#pragma once
#include "contract.h"
namespace engine {
// Transient UI indexes only; persistent identity is mapped by the content table.
inline bool drawDistinct(uint8_t *output, uint8_t count, uint8_t poolSize, const Ports &ports) {
    if (poolSize > 32 || count > poolSize)
        return false;
    uint8_t pool[32];
    for (uint8_t i = 0; i < poolSize; ++i)
        pool[i] = i;
    for (uint8_t i = 0; i < count; ++i) {
        const uint8_t remaining = poolSize - i;
        const uint32_t random = ports.random ? ports.random(ports.context) : 0;
        const uint8_t pick = random % remaining;
        output[i] = pool[pick];
        pool[pick] = pool[remaining - 1];
    }
    return true;
}
} // namespace engine
