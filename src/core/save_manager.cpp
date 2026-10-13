// src/core/save_manager.cpp

#include "save_manager.h"
#include "../app/save_format.h"
#include "../engine/slot_store.h"
#include "../config/game_config.h"
#include "../pet/gallery.h"
#include "../app/diagnostics.h"
#include "../engine/storage.h"
#include <string.h>

SaveManager saveManager;

static engine::KeyValue& prefs=engine::keyValue;

// NVS key 名称 (slot0/1/2)
static const char* HDR_KEYS[SAVE_SLOT_COUNT]  = { "s0_hdr", "s1_hdr", "s2_hdr" };
static const char* DATA_KEYS[SAVE_SLOT_COUNT] = { "s0_dat", "s1_dat", "s2_dat" };
static const char* GALLERY_KEYS[SAVE_SLOT_COUNT] = { "s0_gal", "s1_gal", "s2_gal" };
static const char* GALLERY_NVS_KEY = "gallery";
static const char* ACTIVE_PAIR_KEY = "slot_cfg";
static const char* DEVICE_STATE_KEY = "dev_state";

// Atomic record keys are separate from untouched legacy v9 keys.
static const char* RECORD_KEYS[3]={"s0_v10","s1_v10","s2_v10"};
static const char* STAGE_KEYS[3]={"s0_stage","s1_stage","s2_stage"};
static size_t readRecord(void*,uint8_t slot,bool staged,uint8_t* bytes,size_t capacity) {
    if(slot>=3)return 0;
    const char* key=staged?STAGE_KEYS[slot]:RECORD_KEYS[slot];
    if(prefs.isKey(key)) {
        const size_t length=prefs.getBytesLength(key);
        if(length>capacity)return 0;
        return prefs.getBytes(key,bytes,capacity);
    }
    if(staged)return 0;
    // Read-only v9 fallback. Corrupt/unknown v10 NEVER silently falls back to old data.
    if(prefs.getBytesLength(HDR_KEYS[slot])!=16||prefs.getBytesLength(DATA_KEYS[slot])!=56)return 0;
    uint8_t legacy[84];
    if(prefs.getBytes(HDR_KEYS[slot],legacy,16)!=16||prefs.getBytes(DATA_KEYS[slot],legacy+16,56)!=56)return 0;
    size_t legacySize=72;
    if(prefs.isKey(GALLERY_KEYS[slot])) {
        if(prefs.getBytesLength(GALLERY_KEYS[slot])!=12 || prefs.getBytes(GALLERY_KEYS[slot],legacy+72,12)!=12)return 0;
        legacySize=84;
    }
    PetState pet;SaveHeader hdr;GalleryData gallery;bool full=false;
    if(app::decodeSave(legacy,legacySize,pet,hdr,&gallery,&full)!=engine::DecodeError::Ok)return 0;
    size_t size=0;
    return app::encodeSave(pet,hdr,full?&gallery:nullptr,bytes,capacity,size)?size:0;
}
static bool writeRecord(void*,uint8_t slot,bool staged,const uint8_t* bytes,size_t size) {
    if(slot>=3)return false;
    return prefs.putBytes(staged?STAGE_KEYS[slot]:RECORD_KEYS[slot],bytes,size)==size;
}
static engine::SlotStore records() {return {{nullptr,readRecord,writeRecord},app::packageContract()};}
static SaveHeader metadata(const engine::Document& doc) {
    SaveHeader hdr{};hdr.version=10;hdr.data_size=engine::PetBytes;hdr.sequence=doc.sequence;hdr.save_time=doc.savedAt;
    uint8_t bytes[engine::PetBytes];size_t size=0;auto petDoc=doc;petDoc.full=false;
    if(engine::encode(petDoc,app::packageContract(),bytes,sizeof(bytes),size)==engine::DecodeError::Ok)hdr.checksum=bytes[22]|(uint16_t(bytes[23])<<8);
    return hdr;
}

const char* SaveManager::slotHdrKey(uint8_t slot) {
    return HDR_KEYS[slot];
}

const char* SaveManager::slotDataKey(uint8_t slot) {
    return DATA_KEYS[slot];
}

const char* SaveManager::slotGalleryKey(uint8_t slot) {
    return GALLERY_KEYS[slot];
}

SaveResult SaveManager::init() {
    _initialized=false;_recoveryRequired=true;
    bool ok = prefs.begin(SAVE_NVS_NAMESPACE, false);
    if (!ok) {
        app::trace("[Save] ERROR: NVS init failed");
        return SAVE_ERR_NVS_INIT;
    }

    _initialized = true;
    _importInProgress=false;_frozenOwner=0xff;_galleryPending=false;_loadedSaveTime=0;
    DeviceState existingDevice;existingDevice.init();
    if(loadDeviceState(existingDevice)==SAVE_OK&&existingDevice.is_visiting)_frozenOwner=existingDevice.owner_frozen_slot;
    _recoveryRequired=hasSave();
    _lastSaveTime = 0;_lastWrittenSlot=0xff;_lastWrittenSequence=0;
    _stickyWriteSlot = 0xFF;
    _activeSlotA = 0;
    _activeSlotB = 1;

    app::trace("[Save] NVS initialized (multi-slot)");

    resyncSequenceFromNvs();
    loadActivePairConfig();

    return SAVE_OK;
}

void SaveManager::resyncSequenceFromNvs() {
    uint32_t maxSeq = 0;
    uint8_t validCount = 0;
    for (uint8_t i = 0; i < SAVE_SLOT_COUNT; i++) {
        SaveHeader hdr;
        if (readSlotHeader(i, hdr)) {
            validCount++;
            if (hdr.sequence > maxSeq) maxSeq = hdr.sequence;
            app::tracef("[Save] 存档%u (slot %u): seq=%lu, ver=%d\n",
                          (unsigned)(i + 1), i, hdr.sequence, hdr.version);
        } else {
            app::tracef("[Save] 存档%u (slot %u): empty/invalid\n", (unsigned)(i + 1), i);
        }
    }

    _nextSequence = maxSeq == UINT32_MAX ? 0 : maxSeq + 1; // 0 means exhausted, never wrap ordering
    if (validCount > 0)
        app::tracef("[Save] Found %d valid slot(s), next seq=%lu\n", validCount, _nextSequence);
    else
        app::trace("[Save] No existing save data");
}

bool SaveManager::isSlotActive(uint8_t slot) const {
    return (slot == _activeSlotA) || (slot == _activeSlotB);
}

void SaveManager::loadActivePairConfig() {
    uint8_t cfg[4]{};

    size_t readLen = prefs.getBytes(ACTIVE_PAIR_KEY, &cfg, sizeof(cfg));
    if (readLen == sizeof(cfg) &&
        cfg[0] < SAVE_SLOT_COUNT &&
        cfg[1] < SAVE_SLOT_COUNT &&
        cfg[0] != cfg[1]) {
        _activeSlotA = cfg[0];
        _activeSlotB = cfg[1];
    } else {
        _activeSlotA = 0;
        _activeSlotB = 1;
        // Startup uses a RAM default; never repair unreadable config implicitly.
    }
    app::tracef("[Save] Active slots: %u,%u\n", _activeSlotA, _activeSlotB);
}

SaveResult SaveManager::persistActivePairConfig() {
    uint8_t cfg[4]{};
    cfg[0] = _activeSlotA;
    cfg[1] = _activeSlotB;
    return prefs.putBytes(ACTIVE_PAIR_KEY, &cfg, sizeof(cfg))==sizeof(cfg)?SAVE_OK:SAVE_ERR_WRITE;
}

SaveResult SaveManager::save(const PetState& pet, uint32_t saveTime) {
    if(!_initialized)return SAVE_ERR_NVS_INIT;
    if(_recoveryRequired||_nextSequence==0)return SAVE_ERR_CORRUPTED;
    if(_galleryPending){const auto result=saveGallery(_pendingGallery);if(result!=SAVE_OK)return result;}
    SaveHeader hdr{};hdr.version=10;hdr.sequence=_nextSequence;hdr.save_time=saveTime;
    const uint8_t slot=getSaveTargetSlot();
    if(slot>=SAVE_SLOT_COUNT)return SAVE_ERR_CORRUPTED;
    GalleryData gallery;gallery.init();const auto galleryResult=loadGallery(gallery);
    if(galleryResult!=SAVE_OK&&galleryResult!=SAVE_ERR_NO_DATA)return galleryResult;
    // A successful pet snapshot always includes its own form, even when the
    // presentation evolution callback has not run yet. Retry a failed global
    // update before allowing a newer pet to replace a valid record.
    if(!gallery.isUnlocked(pet.form)){
        gallery.unlock(pet.form);
        if(galleryResult==SAVE_OK){const auto result=saveGallery(gallery);if(result!=SAVE_OK)return result;}
    }
    const auto doc=app::document(pet,hdr,&gallery);
    if(!records().write(slot,doc)){_stickyWriteSlot=slot;return SAVE_ERR_VERIFY;}
    _lastWrittenSlot=slot;_lastWrittenSequence=_nextSequence;
    _stickyWriteSlot=0xff;++_nextSequence;_loadedSaveTime=saveTime;return SAVE_OK;
}

SaveResult SaveManager::load(PetState& pet) {
    _recoveryRequired=true;
    if(importPending())return SAVE_ERR_CORRUPTED;
    DeviceState device;device.init();
    const auto deviceResult=loadDeviceState(device);
    if(deviceResult!=SAVE_OK&&deviceResult!=SAVE_ERR_NO_DATA)return deviceResult;
    GalleryData globalGallery;globalGallery.init();
    const auto galleryResult=loadGallery(globalGallery);
    if(galleryResult!=SAVE_OK&&galleryResult!=SAVE_ERR_NO_DATA)return galleryResult;
    _frozenOwner=device.is_visiting?device.owner_frozen_slot:0xff;
    if(device.is_visiting && setActivePairForVisit(device.owner_frozen_slot)!=SAVE_OK)return SAVE_ERR_WRITE;
    const auto recoverableSlot=[&](uint8_t slot){return !device.is_visiting||slot!=device.owner_frozen_slot;};
    if (!_initialized) return SAVE_ERR_NVS_INIT;

    // 先全槽扫描：若“有且仅有一个有效槽”，固定从该槽加载并重建活动槽对
    struct FullSlotInfo {
        bool valid;
        SaveHeader hdr;
    };
    FullSlotInfo full[SAVE_SLOT_COUNT];
    uint8_t validCount = 0;
    uint8_t onlyValidSlot = 0xFF;
    for (uint8_t i = 0; i < SAVE_SLOT_COUNT; i++) {
        SaveHeader h;
        bool ok = recoverableSlot(i) && readSlotHeader(i, h);
        full[i].valid = ok;
        if (ok) {
            full[i].hdr = h;
            validCount++;
            onlyValidSlot = i;
        }
    }
    if (validCount == 1 && onlyValidSlot < SAVE_SLOT_COUNT) {
        app::tracef("[Save] Only one valid slot found: %u. Forcing load from it.\n", onlyValidSlot);
        if(!device.is_visiting && setActivePairForImportedBaseSlot(onlyValidSlot)!=SAVE_OK)return SAVE_ERR_WRITE;
        SaveResult r = loadSlot(onlyValidSlot, pet);
        if (r == SAVE_OK) {
            _loadedSaveTime = full[onlyValidSlot].hdr.save_time;
            app::tracef("[Save] Loaded from only valid slot %u (save_time=%lu)\n",
                          onlyValidSlot, _loadedSaveTime);
            _recoveryRequired=false;
            return SAVE_OK;
        }
        app::tracef("[Save] Only valid slot %u failed to load (err=%d)\n", onlyValidSlot, r);
        return r;
    }

    // 收集活动槽对中的有效槽头，按 sequence 从大到小尝试
    struct SlotInfo {
        uint8_t slot;
        uint32_t sequence;
        bool valid;
    };
    SlotInfo slots[SAVE_SLOT_COUNT];

    for (uint8_t i = 0; i < SAVE_SLOT_COUNT; i++) {
        SaveHeader hdr;
        slots[i].slot = i;
        if (!isSlotActive(i)||!recoverableSlot(i)) {
            slots[i].valid = false;
            slots[i].sequence = 0;
            continue;
        }
        slots[i].valid = readSlotHeader(i, hdr);
        slots[i].sequence = slots[i].valid ? hdr.sequence : 0;
    }

    // 简单排序: 按 sequence 降序 (新的在前)
    for (uint8_t i = 0; i < SAVE_SLOT_COUNT - 1; i++) {
        for (uint8_t j = i + 1; j < SAVE_SLOT_COUNT; j++) {
            if (slots[j].sequence > slots[i].sequence) {
                SlotInfo tmp = slots[i];
                slots[i] = slots[j];
                slots[j] = tmp;
            }
        }
    }

    // 按新到旧顺序尝试加载（活动槽对）
    bool anyActiveValid = false;
    for (uint8_t i = 0; i < SAVE_SLOT_COUNT; i++) {
        if (!slots[i].valid) continue;
        anyActiveValid = true;

        app::tracef("[Save] Trying slot %d (seq=%lu)...\n", slots[i].slot, slots[i].sequence);
        SaveResult r = loadSlot(slots[i].slot, pet);
        if (r == SAVE_OK) {
            // 读取该槽的 save_time
            SaveHeader hdr;
            if (readSlotHeader(slots[i].slot, hdr)) {
                _loadedSaveTime = hdr.save_time;
            }
            app::tracef("[Save] Loaded from slot %d (save_time=%lu)\n", slots[i].slot, _loadedSaveTime);
            _recoveryRequired=false;
            return SAVE_OK;
        }
        app::tracef("[Save] Slot %d failed (err=%d), trying next...\n", slots[i].slot, r);
    }

    // 兜底：活动槽对失败后，扫描全槽尝试恢复（配置异常/历史残留时避免“有档读不到”）
    app::trace("[Save] Active-pair load failed, trying full-slot fallback...");
    struct FallbackInfo {
        uint8_t slot;
        uint32_t sequence;
        bool valid;
    };
    FallbackInfo fb[SAVE_SLOT_COUNT];
    for (uint8_t i = 0; i < SAVE_SLOT_COUNT; i++) {
        SaveHeader hdr;
        fb[i].slot = i;
        fb[i].valid = recoverableSlot(i) && readSlotHeader(i, hdr);
        fb[i].sequence = fb[i].valid ? hdr.sequence : 0;
    }
    for (uint8_t i = 0; i < SAVE_SLOT_COUNT - 1; i++) {
        for (uint8_t j = i + 1; j < SAVE_SLOT_COUNT; j++) {
            if (fb[j].sequence > fb[i].sequence) {
                FallbackInfo tmp = fb[i];
                fb[i] = fb[j];
                fb[j] = tmp;
            }
        }
    }
    for (uint8_t i = 0; i < SAVE_SLOT_COUNT; i++) {
        if (!fb[i].valid) continue;
        SaveResult r = loadSlot(fb[i].slot, pet);
        if (r == SAVE_OK) {
            SaveHeader hdr;
            if (readSlotHeader(fb[i].slot, hdr)) {
                _loadedSaveTime = hdr.save_time;
            }
            app::tracef("[Save] Fallback loaded slot %d (seq=%lu, save_time=%lu)\n",
                          fb[i].slot, fb[i].sequence, _loadedSaveTime);
            // 自动修复活动槽对：以该槽为导入基槽套用通用规则
            if(!device.is_visiting && setActivePairForImportedBaseSlot(fb[i].slot)!=SAVE_OK)return SAVE_ERR_WRITE;
            _recoveryRequired=false;
            return SAVE_OK;
        }
        app::tracef("[Save] Fallback slot %d failed (err=%d)\n", fb[i].slot, r);
    }

    if (!anyActiveValid) {
        app::trace("[Save] Active pair had no valid header, and fallback found no recoverable save");
    } else {
        app::trace("[Save] Active pair had headers but all loads failed; fallback also failed");
    }
    return SAVE_ERR_NO_DATA;
}

// expectedEpoch 须与触发本次校验前对 save() 传入的 saveTime 相同（不在此重采 timeManager）
bool SaveManager::verifyLatestSave(uint32_t expectedEpoch, const PetState& pet) {
    return _initialized && _lastWrittenSlot<SAVE_SLOT_COUNT &&
        verifyWrittenSlot(_lastWrittenSlot,expectedEpoch,_lastWrittenSequence,pet);
}

SaveResult SaveManager::loadSlot(uint8_t slot, PetState& pet) {
    engine::Document doc;
    if(records().read(slot,doc)!=engine::DecodeError::Ok)return SAVE_ERR_CORRUPTED;
    pet=selected::project(doc.state);return SAVE_OK;
}

SaveResult SaveManager::loadFromSlot(uint8_t slot, PetState& pet) {
    if(importPending()&&!_importInProgress)return SAVE_ERR_CORRUPTED;
    if (!_initialized) return SAVE_ERR_NVS_INIT;
    if (slot >= SAVE_SLOT_COUNT) return SAVE_ERR_CORRUPTED;

    SaveResult r = loadSlot(slot, pet);
    if (r != SAVE_OK) return r;

    SaveHeader hdr;
    if (!readSlotHeader(slot, hdr)) return SAVE_ERR_CORRUPTED;
    _loadedSaveTime = hdr.save_time;
    _recoveryRequired=false;
    return SAVE_OK;
}

SaveResult SaveManager::exportSlotRaw(uint8_t slot, SaveHeader& hdr, PetState& pet) {
    if(!_initialized)return SAVE_ERR_NVS_INIT;
    engine::Document doc;
    if(records().read(slot,doc)!=engine::DecodeError::Ok)return SAVE_ERR_CORRUPTED;
    pet=selected::project(doc.state);hdr=metadata(doc);return SAVE_OK;
}

SaveResult SaveManager::exportSlotRawWithGallery(uint8_t slot, SaveHeader& hdr, PetState& pet,
                                                 GalleryData& gallery, bool& galleryBoundToSlot) {
    SaveResult r = exportSlotRaw(slot, hdr, pet);
    if (r != SAVE_OK) return r;

    if (loadSlotGallery(slot, gallery)) {
        galleryBoundToSlot = true;
        return SAVE_OK;
    }

    // 旧版本没有槽位图鉴快照时，构造最小一致图鉴，避免“旧宠物+新图鉴”混搭
    gallery.init();
    gallery.unlock(pet.form);
    galleryBoundToSlot = false;
    return SAVE_OK;
}

bool SaveManager::getSlotStatus(uint8_t slot, SaveHeader& hdr, SaveResult& status) {
    if (!_initialized) {
        status = SAVE_ERR_NVS_INIT;
        return false;
    }
    if (slot >= SAVE_SLOT_COUNT) {
        status = SAVE_ERR_CORRUPTED;
        return false;
    }
    PetState temp;
    status = exportSlotRaw(slot, hdr, temp);
    return status == SAVE_OK;
}

SaveResult SaveManager::importSlotRaw(uint8_t slot, const SaveHeader& hdrIn, const PetState& pet, bool forceNewest) {
    if(importPending()&&!_importInProgress)return SAVE_ERR_CORRUPTED;
    if(!_initialized)return SAVE_ERR_NVS_INIT;
    if(slot>=3)return SAVE_ERR_CORRUPTED;
    if(hdrIn.version!=10)return SAVE_ERR_VERSION_MISMATCH;
    if(hdrIn.data_size!=engine::PetBytes)return SAVE_ERR_CORRUPTED;
    auto doc=app::document(pet,hdrIn);
    if(engine::validateDocument(doc,app::packageContract())!=engine::DecodeError::Ok)return SAVE_ERR_CORRUPTED;
    if(metadata(doc).checksum!=hdrIn.checksum)return SAVE_ERR_CHECKSUM;
    if(forceNewest){resyncSequenceFromNvs();if(_nextSequence==0)return SAVE_ERR_CORRUPTED;doc.sequence=_nextSequence;}
    if(!records().write(slot,doc))return SAVE_ERR_VERIFY;
    _recoveryRequired=false;_stickyWriteSlot=0xff;resyncSequenceFromNvs();return SAVE_OK;
}

SaveResult SaveManager::setActivePairForImportedBaseSlot(uint8_t baseSlot) {
    if (baseSlot >= SAVE_SLOT_COUNT) {
        return SAVE_ERR_CORRUPTED;
    }

    // 导入完整备份时：不需要保留现有存档
    // 导入槽 baseSlot 必入活跃槽对
    // 在另外两个槽中选"更旧"的那个入活跃槽对（后续轮换写入）
    // 另一个槽冻结（不参与自动/手动 save 轮换）
    uint8_t others[2];
    uint8_t k = 0;
    for (uint8_t i = 0; i < SAVE_SLOT_COUNT; i++) {
        if (i == baseSlot) continue;
        others[k++] = i;
    }

    struct SlotAgeInfo {
        uint8_t slot;
        bool valid;
        uint32_t seq;
        uint32_t saveTime;
    };

    auto readAge = [&](uint8_t slot) -> SlotAgeInfo {
        SaveHeader hdr;
        bool ok = readSlotHeader(slot, hdr);
        SlotAgeInfo info{};
        info.slot = slot;
        info.valid = ok;
        info.seq = ok ? hdr.sequence : 0;
        info.saveTime = ok ? hdr.save_time : 0;
        return info;
    };

    auto olderThan = [&](const SlotAgeInfo& a, const SlotAgeInfo& b) -> bool {
        if (a.valid != b.valid) return !a.valid;
        if (!a.valid && !b.valid) return a.slot < b.slot;
        if (a.seq != b.seq) return a.seq < b.seq;
        if (a.saveTime != b.saveTime) return a.saveTime < b.saveTime;
        return a.slot < b.slot;
    };

    SlotAgeInfo o0 = readAge(others[0]);
    SlotAgeInfo o1 = readAge(others[1]);
    uint8_t older = olderThan(o0, o1) ? o0.slot : o1.slot;
    uint8_t frozen = (older == others[0]) ? others[1] : others[0];

    _activeSlotA = baseSlot;
    _activeSlotB = older;
    if(persistActivePairConfig()!=SAVE_OK){requireRecovery();return SAVE_ERR_WRITE;}
    app::tracef("[Save] Active slots updated after import: %u,%u (frozen=%u)\n",
                  _activeSlotA, _activeSlotB, frozen);
    return SAVE_OK;
}

SaveResult SaveManager::setActivePairForVisit(uint8_t ownerFrozenSlot) {
    if (ownerFrozenSlot >= SAVE_SLOT_COUNT) return SAVE_ERR_CORRUPTED;

    // 串门：冻结主人存档所在槽，其余两槽作为访客宠物的轮转写入槽
    uint8_t others[2];
    uint8_t k = 0;
    for (uint8_t i = 0; i < SAVE_SLOT_COUNT; i++) {
        if (i == ownerFrozenSlot) continue;
        others[k++] = i;
    }
    _frozenOwner=ownerFrozenSlot;
    _activeSlotA = others[0];
    _activeSlotB = others[1];
    if(persistActivePairConfig()!=SAVE_OK){requireRecovery();return SAVE_ERR_WRITE;}
    app::tracef("[Save] Active slots for visit: %u,%u (owner frozen=%u)\n",
                  _activeSlotA, _activeSlotB, ownerFrozenSlot);
    return SAVE_OK;
}

SaveResult SaveManager::setActivePairAfterVisitEnd(uint8_t ownerSlot) {
    if (ownerSlot >= SAVE_SLOT_COUNT) return SAVE_ERR_CORRUPTED;

    // 串门结束：主人存档槽回到活跃对，另一个槽选更旧的
    const auto result=setActivePairForImportedBaseSlot(ownerSlot);
    if(result!=SAVE_OK)return result;
    _frozenOwner=0xff;
    app::tracef("[Save] Active slots restored after visit end (owner=%u)\n", ownerSlot);
    return SAVE_OK;
}

SaveResult SaveManager::setActivePair(uint8_t a, uint8_t b) {
    if (a >= SAVE_SLOT_COUNT || b >= SAVE_SLOT_COUNT || a == b) {
        app::tracef("[Save] WARN: setActivePair(%u,%u) invalid, reloading from NVS\n", a, b);
        loadActivePairConfig();
        return SAVE_ERR_CORRUPTED;
    }
    _activeSlotA = a;
    _activeSlotB = b;
    if(persistActivePairConfig()!=SAVE_OK){requireRecovery();return SAVE_ERR_WRITE;}
    app::tracef("[Save] Active slots set directly: %u,%u\n", _activeSlotA, _activeSlotB);
    return SAVE_OK;
}

bool SaveManager::readSlotHeader(uint8_t slot, SaveHeader& hdr) {
    if(!_initialized)return false;
    engine::Document doc;
    if(records().read(slot,doc)!=engine::DecodeError::Ok)return false;
    hdr=metadata(doc);return true;
}

bool SaveManager::loadSlotGallery(uint8_t slot, GalleryData& gallery) {
    engine::Document doc;
    if(records().read(slot,doc)!=engine::DecodeError::Ok||!doc.full)return false;
    GalleryData g;g.init();for(uint8_t i=0;i<doc.gallery.size;++i)g.unlock(static_cast<Form>(doc.gallery.ids[i]));gallery=g;return true;
}

SaveResult SaveManager::syncGlobalGalleryToSlot(uint8_t slot) {
    if(!_initialized||_recoveryRequired)return SAVE_ERR_CORRUPTED;
    engine::Document doc;
    if(records().read(slot,doc)!=engine::DecodeError::Ok)return SAVE_ERR_READ;
    GalleryData gallery;const auto result=loadGallery(gallery);if(result!=SAVE_OK)return result;
    doc.gallery={};for(size_t i=0;i<content::FormCount;++i)if(gallery.isUnlocked(content::formAt(i)))doc.gallery.unlock(content::Forms[i].id);
    doc.full=true;return records().write(slot,doc)?SAVE_OK:SAVE_ERR_WRITE;
}

uint8_t SaveManager::getOldestSlot() {
    // Only known valid or truly absent slots are writable; frozen/unknown/corrupt records are excluded.
    uint32_t minSeq = UINT32_MAX;
    uint8_t oldest = 0xff;

    for (uint8_t i = 0; i < SAVE_SLOT_COUNT; i++) {
        if (!isSlotActive(i)||!slotWritable(i)) continue;
        SaveHeader hdr;
        uint32_t seq = 0;
        if (readSlotHeader(i, hdr)) {
            seq = hdr.sequence;
        }
        // 空槽 seq=0, 一定最小
        if (oldest==0xff||seq < minSeq) {
            minSeq = seq;
            oldest = i;
        }
    }
    return oldest;
}

uint8_t SaveManager::getSaveTargetSlot() {
    if (_stickyWriteSlot < SAVE_SLOT_COUNT && slotWritable(_stickyWriteSlot))
        return _stickyWriteSlot;
    return getOldestSlot();
}

bool SaveManager::verifyWrittenSlot(uint8_t slot, uint32_t saveTime, uint32_t expectedSeq,
                                    const PetState& pet) {
    engine::Document doc;
    if(records().read(slot,doc)!=engine::DecodeError::Ok||doc.sequence!=expectedSeq||doc.savedAt!=saveTime)return false;
    const auto expected=app::document(pet,metadata(doc));
    uint8_t a[engine::PetBytes],b[engine::PetBytes];size_t na=0,nb=0;doc.full=false;
    return engine::encode(doc,app::packageContract(),a,sizeof(a),na)==engine::DecodeError::Ok&&engine::encode(expected,app::packageContract(),b,sizeof(b),nb)==engine::DecodeError::Ok&&na==nb&&!memcmp(a,b,na);
}

bool SaveManager::slotHasData(uint8_t slot) const {
    return prefs.isKey(RECORD_KEYS[slot])||prefs.isKey(HDR_KEYS[slot])||prefs.isKey(DATA_KEYS[slot])||prefs.isKey(GALLERY_KEYS[slot]);
}
bool SaveManager::slotWritable(uint8_t slot) {
    if(slot>=SAVE_SLOT_COUNT||slot==_frozenOwner)return false;
    if(!slotHasData(slot))return true;
    SaveHeader header;
    // Existing but unreadable/unknown is never an empty automatic-write target.
    return readSlotHeader(slot,header);
}
bool SaveManager::hasSave() {
    if(!_initialized)return false;
    for(uint8_t i=0;i<3;++i)if(slotHasData(i))return true;
    for(const auto* key:{"gallery","dev_state","gallery_v10","device_v10","owner_restore"})if(prefs.isKey(key))return true;
    return importPending();
}
namespace {
bool writeImportGuard(uint8_t status,uint8_t owner) {
    uint8_t bytes[6]={0xa7,status,owner,1,0,0};
    const auto crc=engine::checksum(bytes,4);bytes[4]=uint8_t(crc);bytes[5]=uint8_t(crc>>8);
    return prefs.putBytes("import_guard",bytes,sizeof(bytes))==sizeof(bytes);
}
}
bool SaveManager::importPending() const {
    if(!prefs.isKey("import_guard"))return false;
    uint8_t guard[6];
    if(prefs.getBytesLength("import_guard")!=6||prefs.getBytes("import_guard",guard,6)!=6)return true;
    const auto crc=engine::checksum(guard,4);
    if(guard[0]!=0xa7||guard[3]!=1||guard[4]!=uint8_t(crc)||guard[5]!=uint8_t(crc>>8))return true;
    return guard[1]!=0||guard[2]!=255;
}
SaveResult SaveManager::beginImport(uint8_t owner) {
    if(!_initialized||importPending()||(owner>=SAVE_SLOT_COUNT&&owner!=0xff))return SAVE_ERR_CORRUPTED;
    if(!writeImportGuard(1,owner)){requireRecovery();return SAVE_ERR_WRITE;}
    _frozenOwner=owner;_importInProgress=true;_recoveryRequired=false;return SAVE_OK;
}
SaveResult SaveManager::finishImport(uint8_t frozenOwnerAfter) {
    if(frozenOwnerAfter>=SAVE_SLOT_COUNT && frozenOwnerAfter!=0xff)return SAVE_ERR_CORRUPTED;
    if(!_importInProgress)return SAVE_ERR_CORRUPTED;
    if(!writeImportGuard(0,255)){requireRecovery();return SAVE_ERR_WRITE;}
    _importInProgress=false;_frozenOwner=frozenOwnerAfter;return SAVE_OK;
}

SaveResult SaveManager::beginOwnerRestore(uint8_t owner) {
    if(owner>=SAVE_SLOT_COUNT)return SAVE_ERR_CORRUPTED;
    auto result=beginImport(owner);
    if(result!=SAVE_OK)return result;
    uint8_t bytes[engine::MaxSaveBytes],verify[engine::MaxSaveBytes];
    const auto size=readRecord(nullptr,owner,false,bytes,sizeof(bytes));
    engine::Document original;
    if(engine::decode(bytes,size,app::packageContract(),original)!=engine::DecodeError::Ok)
        return SAVE_ERR_CORRUPTED;
    if(prefs.putBytes("owner_restore",bytes,size)!=size)return SAVE_ERR_WRITE;
    if(prefs.getBytesLength("owner_restore")!=size ||
       prefs.getBytes("owner_restore",verify,sizeof(verify))!=size || memcmp(bytes,verify,size)!=0)
        return SAVE_ERR_VERIFY;
    return SAVE_OK;
}

SaveResult SaveManager::saveRestoredOwner(uint8_t owner,const PetState& pet,uint32_t now) {
    if(!_importInProgress||_recoveryRequired||owner>=SAVE_SLOT_COUNT)return SAVE_ERR_CORRUPTED;
    GalleryData gallery;gallery.init();
    const auto result=loadGallery(gallery);
    if(result!=SAVE_OK&&result!=SAVE_ERR_NO_DATA)return result;
    resyncSequenceFromNvs();
    if(_nextSequence==0)return SAVE_ERR_CORRUPTED;
    SaveHeader header{};header.version=10;header.sequence=_nextSequence;header.save_time=now;
    if(!records().write(owner,app::document(pet,header,&gallery)))return SAVE_ERR_VERIFY;
    _lastWrittenSlot=owner;_lastWrittenSequence=_nextSequence;
    _stickyWriteSlot=0xff;++_nextSequence;_loadedSaveTime=now;
    return SAVE_OK;
}

SaveResult SaveManager::exportOwnerBackup(uint8_t* bytes,size_t capacity,size_t& size) {
    size=0;
    if(!_initialized)return SAVE_ERR_NVS_INIT;
    if(!prefs.isKey("owner_restore"))return SAVE_ERR_NO_DATA;
    const auto length=prefs.getBytesLength("owner_restore");
    if(length>capacity||length==0)return SAVE_ERR_CORRUPTED;
    if(prefs.getBytes("owner_restore",bytes,capacity)!=length)return SAVE_ERR_READ;
    engine::Document doc;
    if(engine::decode(bytes,length,app::packageContract(),doc)!=engine::DecodeError::Ok)return SAVE_ERR_CORRUPTED;
    // A pet-only original (including migrated v9 without a bound gallery) must
    // remain restorable as an owner on an empty device, rather than starting a
    // visit with no owner. Only the exported buffer changes, never the NVS copy.
    if(!doc.full){
        doc.full=true;doc.gallery.unlock(doc.state.form);
        return engine::encode(doc,app::packageContract(),bytes,capacity,size)==engine::DecodeError::Ok?SAVE_OK:SAVE_ERR_CORRUPTED;
    }
    size=length;return SAVE_OK;
}

SaveResult SaveManager::erase() {
    if (!_initialized) return SAVE_ERR_NVS_INIT;

    if(!prefs.clear())return SAVE_ERR_WRITE;
    _recoveryRequired=false;_importInProgress=false;_frozenOwner=0xff;_galleryPending=false;_loadedSaveTime=0;
    _nextSequence = 1;_lastWrittenSlot=0xff;_lastWrittenSequence=0;
    _stickyWriteSlot = 0xFF;
    _activeSlotA = 0;
    _activeSlotB = 1;
    if(persistActivePairConfig()!=SAVE_OK){requireRecovery();return SAVE_ERR_WRITE;}
    app::trace("[Save] All save data erased");
    app::trace("[Save] Active slots reset to default: 0,1");

    return SAVE_OK;
}

bool SaveManager::shouldAutoSave(uint32_t currentTime) {
    return !_recoveryRequired&&app::runtime.saveDue(AUTO_SAVE_INTERVAL_SEC);
}

void SaveManager::markSaved(uint32_t currentTime) {
    _lastSaveTime=currentTime;app::runtime.markSaved();
}

uint16_t SaveManager::calcChecksum(const uint8_t* data, size_t len) {
    uint16_t sum = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        sum ^= data[i];
        sum = (sum << 3) | (sum >> 13);  // rotate left 3
        sum += data[i];
    }
    return sum;
}

// ============================================================================
//  图鉴存档 (独立 NVS key, 与主存档分离)
// ============================================================================

SaveResult SaveManager::saveGallery(const GalleryData& gallery) {
    if(!_initialized)return SAVE_ERR_NVS_INIT;
    if(_recoveryRequired)return SAVE_ERR_CORRUPTED;
    _pendingGallery=gallery;_galleryPending=true;
    uint8_t bytes[76]{};engine::Writer w{bytes,sizeof(bytes)};w.u32(content::PackageId);w.u16(1);w.u8(gallery.getUnlockedCount());
    for(size_t i=0;i<content::FormCount;++i)if(gallery.isUnlocked(content::formAt(i)))w.u16(content::Forms[i].id);
    w.u16(engine::checksum(bytes,w.size));
    if(prefs.putBytes("gallery_v10",bytes,w.size)!=w.size)return SAVE_ERR_WRITE;
    uint8_t verify[76];
    if(prefs.getBytesLength("gallery_v10")!=w.size || prefs.getBytes("gallery_v10",verify,sizeof(verify))!=w.size || memcmp(bytes,verify,w.size))return SAVE_ERR_VERIFY;
    _galleryPending=false;return SAVE_OK;
}

SaveResult SaveManager::loadGallery(GalleryData& gallery) {
    if(!_initialized)return SAVE_ERR_NVS_INIT;
    GalleryData temp;temp.init();uint8_t bytes[76];
    if(prefs.isKey("gallery_v10")){
        const size_t length=prefs.getBytesLength("gallery_v10");
        if(length<9||length>sizeof(bytes)||prefs.getBytes("gallery_v10",bytes,sizeof(bytes))!=length)return SAVE_ERR_CORRUPTED;
        engine::Reader r{bytes,length};if(r.u32()!=content::PackageId||r.u16()!=1)return SAVE_ERR_VERSION_MISMATCH;
        const auto count=r.u8();if(count>engine::GalleryCapacity||length!=size_t(9+2*count))return SAVE_ERR_CORRUPTED;
        for(uint8_t i=0;i<count;++i){const auto id=r.u16();if(!content::knownForm(id)||temp.isUnlocked(static_cast<Form>(id)))return SAVE_ERR_CORRUPTED;temp.unlock(static_cast<Form>(id));}
        const auto expected=engine::checksum(bytes,length-2);if(r.u16()!=expected)return SAVE_ERR_CHECKSUM;
    }else if(prefs.isKey(GALLERY_NVS_KEY)){
        if(content::PackageId!=0x41525452)return SAVE_ERR_VERSION_MISMATCH;
        if(prefs.getBytesLength(GALLERY_NVS_KEY)!=12)return SAVE_ERR_CORRUPTED;
        uint8_t old[12];if(prefs.getBytes(GALLERY_NVS_KEY,old,12)!=12)return SAVE_ERR_READ;
        for(uint8_t i=0;i<12;++i){if(old[i]>1)return SAVE_ERR_CORRUPTED;if(old[i])temp.unlock(static_cast<Form>(i));}
    }else {gallery=temp;return SAVE_ERR_NO_DATA;}
    gallery=temp;return SAVE_OK;
}

// ============================================================================
//  设备态 (独立 NVS key, 绑定本机)
// ============================================================================

SaveResult SaveManager::saveDeviceState(const DeviceState& ds) {
    if(ds.is_visiting&&ds.owner_frozen_slot>=3)return SAVE_ERR_CORRUPTED;
    if(!_initialized)return SAVE_ERR_NVS_INIT;
    if(_recoveryRequired)return SAVE_ERR_CORRUPTED;
    uint8_t bytes[19];engine::Writer w{bytes,sizeof(bytes)};w.u8(1);w.u32(content::PackageId);w.u32(ds.device_clock_epoch);w.u16(ds.rounds);w.u8(ds.is_visiting);w.u8(ds.owner_frozen_slot);w.u32(ds.visit_start_epoch);w.u16(engine::checksum(bytes,w.size));
    if(prefs.putBytes("device_v10",bytes,w.size)!=w.size)return SAVE_ERR_WRITE;
    uint8_t verify[19];
    return prefs.getBytesLength("device_v10")==w.size && prefs.getBytes("device_v10",verify,sizeof(verify))==w.size && !memcmp(bytes,verify,w.size)?SAVE_OK:SAVE_ERR_VERIFY;
}

SaveResult SaveManager::loadDeviceState(DeviceState& ds) {
    if(!_initialized)return SAVE_ERR_NVS_INIT;
    DeviceState temp;temp.init();uint8_t bytes[19];
    if(prefs.isKey("device_v10")){
        if(prefs.getBytesLength("device_v10")!=19||prefs.getBytes("device_v10",bytes,sizeof(bytes))!=19)return SAVE_ERR_CORRUPTED;
        engine::Reader r{bytes,sizeof(bytes)};if(r.u8()!=1||r.u32()!=content::PackageId)return SAVE_ERR_VERSION_MISMATCH;
        temp.device_clock_epoch=r.u32();temp.rounds=r.u16();const auto visiting=r.u8();temp.is_visiting=visiting;temp.owner_frozen_slot=r.u8();temp.visit_start_epoch=r.u32();
        if(visiting>1||(temp.is_visiting&&temp.owner_frozen_slot>=3)||r.u16()!=engine::checksum(bytes,17))return SAVE_ERR_CORRUPTED;
    }else if(prefs.isKey(DEVICE_STATE_KEY)){
        if(content::PackageId!=0x41525452)return SAVE_ERR_VERSION_MISMATCH;
        if(prefs.getBytesLength(DEVICE_STATE_KEY)!=12)return SAVE_ERR_CORRUPTED;
        if(prefs.getBytes(DEVICE_STATE_KEY,bytes,12)!=12)return SAVE_ERR_READ;
        engine::Reader r{bytes,12};temp.device_clock_epoch=r.u32();temp.rounds=r.u16();const auto visiting=r.u8();temp.is_visiting=visiting;temp.owner_frozen_slot=r.u8();temp.visit_start_epoch=r.u32();
        if(visiting>1||(temp.is_visiting&&temp.owner_frozen_slot>=3))return SAVE_ERR_CORRUPTED;
    }else{ds=temp;return SAVE_ERR_NO_DATA;}
    ds=temp;return SAVE_OK;
}
