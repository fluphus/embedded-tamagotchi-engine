#pragma once
#include <cstddef>
#include <cstdint>
namespace engine {
struct Writer {
    uint8_t *data;
    size_t capacity, size = 0;
    bool ok = true;
    void u8(uint8_t v) {
        if (size >= capacity) {
            ok = false;
            return;
        }
        data[size++] = v;
    }
    void u16(uint16_t v) {
        u8(v & 255);
        u8(v >> 8);
    }
    void u32(uint32_t v) {
        u16(v & 65535);
        u16(v >> 16);
    }
};
struct Reader {
    const uint8_t *data;
    size_t capacity, size = 0;
    bool ok = true;
    uint8_t u8() {
        if (size >= capacity) {
            ok = false;
            return 0;
        }
        return data[size++];
    }
    uint16_t u16() {
        uint16_t lo = u8();
        return lo | (uint16_t(u8()) << 8);
    }
    uint32_t u32() {
        uint32_t lo = u16();
        return lo | (uint32_t(u16()) << 16);
    }
};
inline uint16_t checksum(const uint8_t *data, size_t size) {
    uint16_t sum = 0xffff;
    for (size_t i = 0; i < size; ++i) {
        sum ^= data[i];
        sum = uint16_t((sum << 3) | (sum >> 13));
        sum = uint16_t(sum + data[i]);
    }
    return sum;
}
} // namespace engine
