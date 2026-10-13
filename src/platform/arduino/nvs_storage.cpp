#include "../../engine/storage.h"
#include <Preferences.h>
namespace {
Preferences nvs;
}
namespace engine {
KeyValue keyValue{
    &nvs,
    [](void *p, const char *ns, bool ro) { return static_cast<Preferences *>(p)->begin(ns, ro); },
    [](void *p, const char *key) { return static_cast<Preferences *>(p)->isKey(key); },
    [](void *p, const char *key) { return static_cast<Preferences *>(p)->getBytesLength(key); },
    [](void *p, const char *key, void *bytes, size_t size) {
        return static_cast<Preferences *>(p)->getBytes(key, bytes, size);
    },
    [](void *p, const char *key, const void *bytes, size_t size) {
        return static_cast<Preferences *>(p)->putBytes(key, bytes, size);
    },
    [](void *p) { return static_cast<Preferences *>(p)->clear(); }};
} // namespace engine
