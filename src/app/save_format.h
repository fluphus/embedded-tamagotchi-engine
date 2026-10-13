#pragma once
#include "../content/artoria/v9.h"
#include "../engine/save_codec.h"
#include "../pet/gallery.h"
#include "runtime.h"
namespace app {
inline engine::PackageContract packageContract() {
    return {content::PackageId, content::knownForm, selected::validate};
}
inline engine::Document document(const PetState &pet, const SaveHeader &hdr,
                                 const GalleryData *gallery = nullptr) {
    engine::Document d;
    d.state = selected::capture(pet);
    d.sequence = hdr.sequence;
    d.savedAt = hdr.save_time;
    d.full = gallery;
    if (gallery)
        for (size_t i = 0; i < content::FormCount; ++i)
            if (gallery->isUnlocked(content::formAt(i)))
                d.gallery.unlock(content::Forms[i].id);
    return d;
}
inline bool encodeSave(const PetState &pet, const SaveHeader &hdr, const GalleryData *gallery,
                       uint8_t *bytes, size_t capacity, size_t &size) {
    return engine::encode(document(pet, hdr, gallery), packageContract(), bytes, capacity, size) ==
           engine::DecodeError::Ok;
}
inline engine::DecodeError decodeSave(const uint8_t *bytes, size_t size, PetState &pet,
                                      SaveHeader &hdr, GalleryData *gallery = nullptr,
                                      bool *full = nullptr) {
    engine::Document d;
    auto error = engine::decode(bytes, size, packageContract(), d);
    if (size == 72 || size == 84) {
        if (content::PackageId != 0x41525452)
            return engine::DecodeError::Package;
        error = artoria::migrateV9(bytes, size, d);
    }
    if (error != engine::DecodeError::Ok)
        return error;
    const auto view = selected::project(d.state);
    SaveHeader metadata{};
    metadata.version = engine::SaveVersion;
    metadata.data_size = engine::PetBytes;
    metadata.sequence = d.sequence;
    metadata.save_time = d.savedAt;
    uint8_t canonical[engine::PetBytes];
    size_t canonicalSize = 0;
    engine::Document petDoc = d;
    petDoc.full = false;
    if (engine::encode(petDoc, packageContract(), canonical, sizeof(canonical), canonicalSize) !=
        engine::DecodeError::Ok)
        return engine::DecodeError::Value;
    metadata.checksum = uint16_t(canonical[22]) | (uint16_t(canonical[23]) << 8);
    if (gallery) {
        GalleryData g;
        g.init();
        for (uint8_t i = 0; i < d.gallery.size; ++i)
            g.unlock(static_cast<Form>(d.gallery.ids[i]));
        *gallery = g;
    }
    pet = view;
    hdr = metadata;
    if (full)
        *full = d.full;
    return engine::DecodeError::Ok;
}
} // namespace app
