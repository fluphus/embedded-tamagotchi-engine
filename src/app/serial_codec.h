#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
inline int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

inline bool decodeHexToBytes(const char* hex, uint8_t* out, size_t maxOut, size_t* outLen) {
    size_t n = strlen(hex);
    if (n == 0 || (n % 2) != 0) return false;
    size_t bytes = n / 2;
    if (bytes > maxOut) return false;
    for (size_t i = 0; i < bytes; i++) {
        int hi = hexNibble(hex[i * 2]);
        int lo = hexNibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    *outLen = bytes;
    return true;
}

