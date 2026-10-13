#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace engine {
using Id = uint16_t;
struct EpochSeconds {
    uint32_t value;
};
struct MonotonicMillis {
    uint32_t value;
};
constexpr size_t ExtensionCapacity = 64;
constexpr size_t GalleryCapacity = 32;
struct State {
    Id form = 0;
    int16_t health = 50;
    int16_t seriousness = 0;
    uint16_t idle_minute_remainder = 0;
    uint32_t idle_paused_until = 0;
    uint32_t last_poke_effect_time = 0;
    uint32_t birth_timestamp = 0;
    uint32_t last_interact_time = 0;
    uint16_t age_days = 0;
    uint8_t stage = 0;
    uint16_t extension_version = 1;
    uint8_t extension_size = 0;
    std::array<uint8_t, ExtensionCapacity> extension{};
};
struct Gallery {
    uint8_t size = 0;
    std::array<Id, GalleryCapacity> ids{};
    bool unlock(Id id) {
        for (uint8_t i = 0; i < size; ++i)
            if (ids[i] == id)
                return true;
        if (size == ids.size())
            return false;
        ids[size++] = id;
        return true;
    }
};
struct Ports {
    void *context = nullptr;
    EpochSeconds (*calendar)(void *) = nullptr;
    MonotonicMillis (*monotonic)(void *) = nullptr;
    uint32_t (*random)(void *) = nullptr;
    void (*diagnostic)(void *, const char *) = nullptr;
};
enum class CommandKind : uint8_t {
    Initialize,
    Poke,
    FeedInteraction,
    Minute,
    AdvanceMinutes,
    Feed,
    SpecialFood,
    DayEffects,
    MissedFeed,
    GrowthCheck,
    EvolutionCheck,
    Reset,
    ContentAction
};
struct Command {
    CommandKind kind = CommandKind::Initialize;
    EpochSeconds time{0};
    uint32_t minutes = 0;
    std::array<Id, 3> foods{};
    Id option = 0;
    uint8_t hour = 0;
};
enum class EventKind : uint8_t { CommandApplied, FormChanged, MetricChanged, SaveDue };
struct Event {
    EventKind kind;
    Id form;
    int16_t health;
    int16_t seriousness;
    EpochSeconds time;
};
// Fixed FIFO with explicit rejection and overflow latch; consumers never execute rules.
template <class T, size_t Capacity> class Queue {
    std::array<T, Capacity> data_{};
    size_t head_ = 0, count_ = 0;
    bool overflow_ = false;

  public:
    bool push(const T &value) {
        if (count_ == Capacity) {
            overflow_ = true;
            return false;
        }
        data_[(head_ + count_) % Capacity] = value;
        ++count_;
        return true;
    }
    bool pop(T &value) {
        if (!count_)
            return false;
        value = data_[head_];
        head_ = (head_ + 1) % Capacity;
        --count_;
        return true;
    }
    size_t size() const { return count_; }
    bool overflowed() const { return overflow_; }
    void clear() {
        head_ = count_ = 0;
        overflow_ = false;
    }
};
} // namespace engine
