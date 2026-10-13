#pragma once
#include <cstddef>
namespace engine {
// Narrow synchronous key-value storage contract; writes replace one key atomically.
struct KeyValue {
    void *context = nullptr;
    bool (*open)(void *, const char *, bool) = nullptr;
    bool (*contains)(void *, const char *) = nullptr;
    size_t (*length)(void *, const char *) = nullptr;
    size_t (*read)(void *, const char *, void *, size_t) = nullptr;
    size_t (*write)(void *, const char *, const void *, size_t) = nullptr;
    bool (*eraseAll)(void *) = nullptr;
    bool begin(const char *name, bool readonly) { return open && open(context, name, readonly); }
    bool isKey(const char *key) { return contains && contains(context, key); }
    size_t getBytesLength(const char *key) { return length ? length(context, key) : 0; }
    size_t getBytes(const char *key, void *data, size_t size) {
        return read ? read(context, key, data, size) : 0;
    }
    size_t putBytes(const char *key, const void *data, size_t size) {
        return write ? write(context, key, data, size) : 0;
    }
    bool clear() { return eraseAll && eraseAll(context); }
};
extern KeyValue keyValue;
} // namespace engine
