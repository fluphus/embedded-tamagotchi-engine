#pragma once
#include <Arduino.h>
#include <cstring>

namespace platform {
inline void diagnosticLine(const char* line) {
#if ARDUINO_USB_CDC_ON_BOOT && ARDUINO_USB_MODE == 1
    // Avoid the queued multi-packet TX tail observed with the pinned HWCDC.
    // Drain sub-packet chunks before adding more. SDK flush is timeout-bounded;
    // this runs on diagnostic/protocol output, never on idle frames.
    for (size_t remaining=std::strlen(line); remaining;) {
        const size_t count=remaining<32?remaining:32;
        if (Serial.write(reinterpret_cast<const uint8_t*>(line),count)!=count) return;
        Serial.flush();
        line+=count;remaining-=count;
    }
    Serial.println();Serial.flush();
#else
    Serial.println(line);
#endif
}
}
