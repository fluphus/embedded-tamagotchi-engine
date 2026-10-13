#pragma once
#include "save_codec.h"
namespace engine {
// Storage must atomically replace a single key, or reject without mutation.
// A stage key is read-verified BEFORE replacing the committed record.
struct Storage {
    void *context;
    size_t (*read)(void *, uint8_t slot, bool staged, uint8_t *, size_t);
    bool (*write)(void *, uint8_t slot, bool staged, const uint8_t *, size_t);
};
class SlotStore {
    Storage storage_;
    PackageContract pack_;

  public:
    SlotStore(Storage storage, PackageContract pack) : storage_(storage), pack_(pack) {}
    DecodeError read(uint8_t slot, Document &doc) const {
        if (slot >= 3)
            return DecodeError::Value;
        uint8_t bytes[MaxSaveBytes];
        const auto count = storage_.read(storage_.context, slot, false, bytes, sizeof(bytes));
        return decode(bytes, count, pack_, doc);
    }
    bool write(uint8_t slot, const Document &doc) const {
        if (slot >= 3)
            return false;
        uint8_t bytes[MaxSaveBytes], verify[MaxSaveBytes];
        size_t size = 0;
        if (encode(doc, pack_, bytes, sizeof(bytes), size) != DecodeError::Ok)
            return false;
        if (!storage_.write(storage_.context, slot, true, bytes, size))
            return false;
        const auto count = storage_.read(storage_.context, slot, true, verify, sizeof(verify));
        if (count != size)
            return false;
        for (size_t i = 0; i < size; ++i)
            if (bytes[i] != verify[i])
                return false;
        return storage_.write(storage_.context, slot, false, bytes, size);
    }
    bool loadPair(uint8_t a, uint8_t b, Document &doc, uint8_t &loaded) const {
        Document first, second;
        const bool goodA = read(a, first) == DecodeError::Ok,
                   goodB = read(b, second) == DecodeError::Ok;
        if (!goodA && !goodB)
            return false;
        if (goodA && (!goodB || first.sequence > second.sequence ||
                      (first.sequence == second.sequence && a < b))) {
            doc = first;
            loaded = a;
        } else {
            doc = second;
            loaded = b;
        }
        return true;
    }
};
} // namespace engine
