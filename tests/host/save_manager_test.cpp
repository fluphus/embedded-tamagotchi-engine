#include "app/save_format.h"
#include "core/save_manager.h"
#include "engine/storage.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <string>
#include <vector>
namespace engine {
KeyValue keyValue;
}
namespace {
int checks = 0;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            std::cerr << __func__ << ':' << __LINE__ << " failed: " #x "\n";                       \
            std::exit(1);                                                                          \
        }                                                                                          \
    } while (false)
struct KV {
    std::map<std::string, std::vector<uint8_t>> values;
    std::string failKey;
    bool corruptStage = false;
    engine::KeyValue port() {
        return {
            this,
            [](void *, const char *, bool) { return true; },
            [](void *p, const char *key) { return static_cast<KV *>(p)->values.count(key) != 0; },
            [](void *p, const char *key) -> size_t {
                auto &m = static_cast<KV *>(p)->values;
                auto i = m.find(key);
                return i == m.end() ? 0 : i->second.size();
            },
            [](void *p, const char *key, void *bytes, size_t capacity) -> size_t {
                auto &kv = *static_cast<KV *>(p);
                auto i = kv.values.find(key);
                if (i == kv.values.end() || i->second.size() > capacity)
                    return 0;
                auto *data = static_cast<uint8_t *>(bytes);
                std::copy(i->second.begin(), i->second.end(), data);
                if (kv.corruptStage && std::string(key).find("stage") != std::string::npos &&
                    i->second.size())
                    data[0] ^= 1;
                return i->second.size();
            },
            [](void *p, const char *key, const void *bytes, size_t size) -> size_t {
                auto &kv = *static_cast<KV *>(p);
                if (kv.failKey == key)
                    return 0;
                const auto *data = static_cast<const uint8_t *>(bytes);
                kv.values[key] = {data, data + size};
                return size;
            },
            [](void *p) {
                static_cast<KV *>(p)->values.clear();
                return true;
            }};
    }
};
PetState fresh() {
    PetState p{};
    p.initNew(1700000000);
    return p;
}
void rotationAndFailure() {
    KV kv;
    engine::keyValue = kv.port();
    SaveManager manager;
    CHECK(manager.init() == SAVE_OK);
    auto pet = fresh();
    CHECK(!manager.hasSave());
    CHECK(manager.save(pet, 1700000000) == SAVE_OK);
    CHECK(manager.save(pet, 1700000300) == SAVE_OK);
    SaveHeader a{}, b{};
    SaveResult result;
    CHECK(manager.getSlotStatus(0, a, result) && manager.getSlotStatus(1, b, result));
    CHECK(a.sequence == 1 && b.sequence == 2);
    const auto slot0 = kv.values["s0_v10"], slot1 = kv.values["s1_v10"];
    kv.failKey = "s0_stage";
    CHECK(manager.save(pet, 1700000600) != SAVE_OK);
    CHECK(kv.values["s0_v10"] == slot0 && kv.values["s1_v10"] == slot1);
    kv.failKey.clear();
    kv.corruptStage = true;
    CHECK(manager.save(pet, 1700000600) != SAVE_OK);
    CHECK(kv.values["s0_v10"] == slot0);
    kv.corruptStage = false;
    kv.failKey = "s0_v10";
    CHECK(manager.save(pet, 1700000600) != SAVE_OK);
    CHECK(kv.values["s0_v10"] == slot0);
    kv.failKey.clear();
    CHECK(manager.save(pet, 1700000600) == SAVE_OK);
    CHECK(manager.getSlotStatus(0, a, result) && a.sequence == 3);
    kv.values["s0_v10"][22] ^= 1;
    SaveManager reloaded;
    CHECK(reloaded.init() == SAVE_OK);
    CHECK(reloaded.load(pet) == SAVE_OK);
    CHECK(reloaded.getLastSaveTime() == 1700000300);
    kv.values["s1_v10"][22] ^= 1;
    SaveManager blocked;
    CHECK(blocked.init() == SAVE_OK);
    CHECK(blocked.hasSave());
    const auto snapshot = kv.values;
    CHECK(blocked.load(pet) != SAVE_OK && blocked.recoveryRequired());
    CHECK(blocked.save(pet, 1700000900) != SAVE_OK);
    CHECK(kv.values == snapshot);
}
void galleryDeviceAndVisit() {
    KV kv;
    engine::keyValue = kv.port();
    SaveManager manager;
    CHECK(manager.init() == SAVE_OK);
    auto pet = fresh();
    GalleryData gallery;
    gallery.init();
    gallery.unlock(pet.form);
    CHECK(manager.saveGallery(gallery) == SAVE_OK);
    CHECK(manager.save(pet, 1700000000) == SAVE_OK);
    SaveHeader hdr{};
    PetState saved;
    GalleryData bound;
    bool hasBound = false;
    CHECK(manager.exportSlotRawWithGallery(0, hdr, saved, bound, hasBound) == SAVE_OK && hasBound &&
          bound.isUnlocked(pet.form));
    GalleryData restored;
    CHECK(manager.loadGallery(restored) == SAVE_OK && restored.isUnlocked(pet.form));
    DeviceState device;
    device.init();
    device.device_clock_epoch = 1700000000;
    device.rounds = 7;
    device.is_visiting = true;
    device.owner_frozen_slot = 0;
    device.visit_start_epoch = 1700000000;
    CHECK(manager.saveDeviceState(device) == SAVE_OK);
    DeviceState read;
    CHECK(manager.loadDeviceState(read) == SAVE_OK && read.is_visiting && read.rounds == 7 &&
          read.owner_frozen_slot == 0);
    manager.setActivePairForVisit(0);
    const auto owner = kv.values["s0_v10"];
    pet.health = 60;
    CHECK(manager.save(pet, 1700000300) == SAVE_OK);
    pet.health = 70;
    CHECK(manager.save(pet, 1700000600) == SAVE_OK);
    CHECK(kv.values["s0_v10"] == owner);
    SaveManager visit;
    CHECK(visit.init() == SAVE_OK && visit.load(pet) == SAVE_OK && pet.health == 70);
    kv.values["s1_v10"][22] ^= 1;
    kv.values["s2_v10"][22] ^= 1;
    SaveManager failedGuest;
    CHECK(failedGuest.init() == SAVE_OK);
    CHECK(failedGuest.load(pet) != SAVE_OK);
    CHECK(kv.values["s0_v10"] == owner); // never load frozen owner as guest fallback
    CHECK(failedGuest.loadFromSlot(0, pet) == SAVE_OK &&
          pet.health == 50); // explicit owner restore
    failedGuest.setActivePairAfterVisitEnd(0);
    CHECK(failedGuest.getActiveSlotA() == 0);
    device.is_visiting = false;
    device.owner_frozen_slot = 255;
    CHECK(failedGuest.saveDeviceState(device) == SAVE_OK);
    CHECK(failedGuest.save(pet, 1700000900) == SAVE_OK);
    kv.values["device_v10"][3] ^= 1;
    SaveManager badDevice;
    CHECK(badDevice.init() == SAVE_OK);
    CHECK(badDevice.load(pet) != SAVE_OK && badDevice.recoveryRequired());
}
void targetSidecars() {
    KV kv;engine::keyValue=kv.port();
    for(const auto &entry: {std::pair<const char*,const char*>{"gallery","gallery.bin"},{"dev_state","device_visit.bin"}}) {
        std::ifstream file(std::string("tests/fixtures/v9/")+entry.second,std::ios::binary);
        kv.values[entry.first]={std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
    }
    SaveManager manager;CHECK(manager.init()==SAVE_OK);
    GalleryData gallery;gallery.init();DeviceState device;device.init();
#if defined(CONTENT_SPROUT) && CONTENT_SPROUT
    CHECK(manager.loadGallery(gallery)!=SAVE_OK);CHECK(manager.loadDeviceState(device)!=SAVE_OK);
#else
    CHECK(manager.loadGallery(gallery)==SAVE_OK&&gallery.getUnlockedCount()==4);
    CHECK(manager.loadDeviceState(device)==SAVE_OK&&device.is_visiting&&device.owner_frozen_slot==0&&device.rounds==7);
#endif
}
void malformedSidecars() {
    KV kv;engine::keyValue=kv.port();SaveManager manager;CHECK(manager.init()==SAVE_OK);
    auto pet=fresh();CHECK(manager.save(pet,1700000000)==SAVE_OK);
    kv.values["gallery"]={1,2};GalleryData gallery;gallery.init();
    CHECK(manager.loadGallery(gallery)!=SAVE_OK);
    const auto before=kv.values;CHECK(manager.save(pet,1700000300)!=SAVE_OK);
    CHECK(kv.values==before);
    SaveManager galleryBlocked;CHECK(galleryBlocked.init()==SAVE_OK);
    CHECK(galleryBlocked.load(pet)!=SAVE_OK&&galleryBlocked.recoveryRequired());
    const auto keepGallery=kv.values;
    CHECK(galleryBlocked.saveGallery(gallery)!=SAVE_OK&&kv.values==keepGallery);
    kv.values.erase("gallery");kv.values["dev_state"]={1,2};
    SaveManager blocked;CHECK(blocked.init()==SAVE_OK);
    CHECK(blocked.load(pet)!=SAVE_OK&&blocked.recoveryRequired());
    const auto protectedBytes=kv.values;CHECK(blocked.save(pet,1700000600)!=SAVE_OK);
    CHECK(kv.values==protectedBytes);
}
void importAndMigration() {
    KV kv;
    engine::keyValue = kv.port();
    SaveManager manager;
    CHECK(manager.init() == SAVE_OK);
    auto pet = fresh();
    CHECK(manager.save(pet, 1700000000) == SAVE_OK);
    SaveHeader hdr{};
    CHECK(manager.exportSlotRaw(0, hdr, pet) == SAVE_OK);
    const auto old = kv.values["s0_v10"];
    auto corrupt = hdr;
    corrupt.checksum ^= 1;
    CHECK(manager.importSlotRaw(0, corrupt, pet, true) == SAVE_ERR_CHECKSUM &&
          kv.values["s0_v10"] == old);
    corrupt = hdr;
    corrupt.version = 99;
    CHECK(manager.importSlotRaw(0, corrupt, pet, true) == SAVE_ERR_VERSION_MISMATCH &&
          kv.values["s0_v10"] == old);
    CHECK(manager.importSlotRaw(1, hdr, pet, true) == SAVE_OK);
    manager.setActivePairForImportedBaseSlot(1);
    CHECK(manager.getActiveSlotA() == 1);
    std::ifstream file("tests/fixtures/v9/adult-white.full.bin", std::ios::binary);
    std::vector<uint8_t> raw((std::istreambuf_iterator<char>(file)), {});
    KV legacy;
    legacy.values["s0_hdr"] = {raw.begin(), raw.begin() + 16};
    legacy.values["s0_dat"] = {raw.begin() + 16, raw.begin() + 72};
    legacy.values["s0_gal"] = {raw.begin() + 72, raw.end()};
    legacy.values["gallery"] = {raw.begin() + 72, raw.end()};
    std::ifstream oldDevice("tests/fixtures/v9/device.bin",std::ios::binary);
    legacy.values["dev_state"]={std::istreambuf_iterator<char>(oldDevice),std::istreambuf_iterator<char>()};
    engine::keyValue = legacy.port();
    SaveManager migration;
    CHECK(migration.init() == SAVE_OK);
#if defined(CONTENT_SPROUT) && CONTENT_SPROUT
    CHECK(migration.load(pet) != SAVE_OK && migration.recoveryRequired());
    const auto keep = legacy.values;
    CHECK(migration.save(pet, 1700000300) != SAVE_OK && legacy.values == keep);
#else
    CHECK(migration.load(pet) == SAVE_OK && pet.health == 73 && pet.mapo_tofu_count == 2);
    DeviceState migratedDevice;CHECK(migration.loadDeviceState(migratedDevice)==SAVE_OK&&migratedDevice.rounds==7&&migratedDevice.device_clock_epoch==1700000000);

    const auto oldHeader = legacy.values["s0_hdr"], oldData = legacy.values["s0_dat"];
    CHECK(migration.save(pet, 1700000300) == SAVE_OK);
    CHECK(legacy.values["s0_hdr"] == oldHeader && legacy.values["s0_dat"] == oldData &&
          legacy.values.count("s1_v10"));
#endif
}
} // namespace
int main() {
    rotationAndFailure();
    galleryDeviceAndVisit();
    importAndMigration();
    malformedSidecars();
    targetSidecars();
    std::cout << "PASS storage " << content::Title << ": " << checks << " checks\n";
}
