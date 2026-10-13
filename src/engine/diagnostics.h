#pragma once
#include "contract.h"
#include <cstdarg>
#include <cstdio>
namespace engine {
namespace support {
inline engine::Ports ports{};
inline uint32_t randomWord() { return ports.random ? ports.random(ports.context) : 0; }
inline void trace(const char *line) {
    if (ports.diagnostic)
        ports.diagnostic(ports.context, line);
}
inline void tracef(const char *format, ...) {
    if(!ports.diagnostic)return;
    char line[192];
    va_list args;
    va_start(args, format);
    std::vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    trace(line);
}
} // namespace support
} // namespace engine
