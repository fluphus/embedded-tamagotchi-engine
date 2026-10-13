#pragma once
#include "bytes.h"
#include "contract.h"
namespace engine {
constexpr size_t PetBytes = 160, FullBytes = 228, MaxSaveBytes = FullBytes;
constexpr uint16_t SaveVersion = 10;
enum class DecodeError : uint8_t {
    Ok,
    Length,
    Magic,
    Version,
    Checksum,
    Package,
    UnknownId,
    Extension,
    Value
};
struct PackageContract {
    uint32_t id;
    bool (*knownForm)(Id);
    bool (*validate)(const State &);
};
struct Document {
    State state{};
    Gallery gallery{};
    uint32_t sequence = 0, savedAt = 0;
    bool full = false;
};
inline uint16_t recordChecksum(const uint8_t *data, size_t size) {
    uint16_t sum = 0xffff;
    for (size_t i = 0; i < size; ++i) {
        uint8_t v = (i == 22 || i == 23) ? 0 : data[i];
        sum ^= v;
        sum = uint16_t((sum << 3) | (sum >> 13));
        sum = uint16_t(sum + v);
    }
    return sum;
}
inline DecodeError validateDocument(const Document &doc, const PackageContract &pack) {
    const auto &s = doc.state;
    if (!pack.knownForm(s.form))
        return DecodeError::UnknownId;
    if (s.extension_size > ExtensionCapacity || !pack.validate(s))
        return DecodeError::Extension;
    if (doc.gallery.size > GalleryCapacity)
        return DecodeError::Value;
    for (uint8_t i = 0; i < doc.gallery.size; ++i) {
        if (!pack.knownForm(doc.gallery.ids[i]))
            return DecodeError::UnknownId;
        for (uint8_t j = 0; j < i; ++j)
            if (doc.gallery.ids[j] == doc.gallery.ids[i])
                return DecodeError::Value;
    }
    return DecodeError::Ok;
}
inline DecodeError encode(const Document &doc, const PackageContract &pack, uint8_t *data,
                          size_t capacity, size_t &size) {
    const auto valid = validateDocument(doc, pack);
    if (valid != DecodeError::Ok)
        return valid;
    const size_t wanted = doc.full ? FullBytes : PetBytes;
    if (capacity < wanted)
        return DecodeError::Length;
    for (size_t i = 0; i < wanted; ++i)
        data[i] = 0;
    Writer w{data, wanted};
    w.u32(0x45544550);
    w.u16(SaveVersion);
    w.u8(doc.full);
    w.u8(0);
    w.u32(pack.id);
    w.u32(doc.sequence);
    w.u32(doc.savedAt);
    w.u16(uint16_t(wanted - 24));
    w.u16(0);
    const auto &s = doc.state;
    w.u16(s.form);
    w.u16(uint16_t(s.health));
    w.u16(uint16_t(s.seriousness));
    w.u16(s.idle_minute_remainder);
    w.u32(s.idle_paused_until);
    w.u32(s.last_poke_effect_time);
    w.u32(s.birth_timestamp);
    w.u32(s.last_interact_time);
    w.u16(s.age_days);
    w.u8(s.stage);
    w.u16(s.extension_version);
    w.u8(s.extension_size);
    for (size_t i = 0; i < ExtensionCapacity; ++i)
        w.u8(i < s.extension_size ? s.extension[i] : 0);
    if (doc.full) {
        w.size = PetBytes;
        w.u8(doc.gallery.size);
        for (size_t i = 0; i < GalleryCapacity; ++i)
            w.u16(i < doc.gallery.size ? doc.gallery.ids[i] : 0);
    }
    const auto crc = recordChecksum(data, wanted);
    data[22] = uint8_t(crc);
    data[23] = uint8_t(crc >> 8);
    size = wanted;
    return w.ok ? DecodeError::Ok : DecodeError::Length;
}
inline DecodeError decode(const uint8_t *data, size_t size, const PackageContract &pack,
                          Document &output) {
    if (size != PetBytes && size != FullBytes)
        return DecodeError::Length;
    Reader r{data, size};
    if (r.u32() != 0x45544550)
        return DecodeError::Magic;
    if (r.u16() != SaveVersion)
        return DecodeError::Version;
    const uint8_t full = r.u8(), reserved = r.u8();
    if (full > 1 || reserved || bool(full) != (size == FullBytes))
        return DecodeError::Value;
    if (r.u32() != pack.id)
        return DecodeError::Package;
    Document d;
    d.full = full;
    d.sequence = r.u32();
    d.savedAt = r.u32();
    if (r.u16() != size - 24)
        return DecodeError::Length;
    if (r.u16() != recordChecksum(data, size))
        return DecodeError::Checksum;
    auto &s = d.state;
    s.form = r.u16();
    s.health = int16_t(r.u16());
    s.seriousness = int16_t(r.u16());
    s.idle_minute_remainder = r.u16();
    s.idle_paused_until = r.u32();
    s.last_poke_effect_time = r.u32();
    s.birth_timestamp = r.u32();
    s.last_interact_time = r.u32();
    s.age_days = r.u16();
    s.stage = r.u8();
    s.extension_version = r.u16();
    s.extension_size = r.u8();
    if (s.extension_size > ExtensionCapacity)
        return DecodeError::Extension;
    for (size_t i = 0; i < ExtensionCapacity; ++i) {
        s.extension[i] = r.u8();
        if (i >= s.extension_size && s.extension[i])
            return DecodeError::Extension;
    }
    while (r.size < PetBytes)
        if (r.u8())
            return DecodeError::Value;
    if (d.full) {
        d.gallery.size = r.u8();
        if (d.gallery.size > GalleryCapacity)
            return DecodeError::Value;
        for (size_t i = 0; i < GalleryCapacity; ++i) {
            d.gallery.ids[i] = r.u16();
            if (i >= d.gallery.size && d.gallery.ids[i])
                return DecodeError::Value;
        }
        while (r.size < size)
            if (r.u8())
                return DecodeError::Value;
    }
    const auto valid = validateDocument(d, pack);
    if (valid != DecodeError::Ok)
        return valid;
    if (!r.ok)
        return DecodeError::Length;
    output = d;
    return DecodeError::Ok;
}
} // namespace engine
