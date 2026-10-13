#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace engine {
// Non-blocking bounded playback mechanics. Traits belong to presentation.
template <class Node, class Traits, size_t Capacity> class Timeline {
    std::array<Node, Capacity> buffer_{};
    size_t head_ = 0, count_ = 0;
    bool playing_ = false, waiting_ = false, overflow_ = false;
    uint32_t started_ = 0;
    uint32_t (*now_)() = nullptr;
    void (*complete_)() = nullptr;
    void (*change_)(const Node *, const Node *) = nullptr;
    uint32_t now() const { return now_ ? now_() : 0; }
    void start(uint32_t timestamp) {
        if (!count_) {
            playing_ = false;
            return;
        }
        playing_ = true;
        started_ = timestamp;
        const auto &n = buffer_[head_];
        waiting_ = Traits::wait(n);
        Traits::start(n);
        if (change_)
            change_(&n, nullptr);
    }
    void advance(uint32_t timestamp) {
        if (!count_)
            return;
        const Node *previous = &buffer_[head_];
        Traits::complete(*previous);
        head_ = (head_ + 1) % Capacity;
        --count_;
        if (!count_) {
            playing_ = waiting_ = false;
            if (change_)
                change_(nullptr, previous);
            if (complete_)
                complete_();
        } else {
            if (change_)
                change_(&buffer_[head_], previous);
            start(timestamp);
        }
    }

  public:
    using QueueCompleteCallback = void (*)();
    using NodeChangeCallback = void (*)(const Node *, const Node *);
    explicit Timeline(uint32_t (*clock)() = nullptr) : now_(clock) {}
    void init() {
        clear();
        complete_ = nullptr;
        change_ = nullptr;
    }
    bool enqueue(const Node &node) {
        if (count_ == Capacity) {
            overflow_ = true;
            return false;
        }
        buffer_[(head_ + count_) % Capacity] = node;
        ++count_;
        if (!playing_ && count_ == 1)
            start(now());
        return true;
    }
    void clear() {
        head_ = count_ = 0;
        playing_ = waiting_ = overflow_ = false;
        started_ = 0;
    }
    bool isEmpty() const { return count_ == 0; }
    bool isPlaying() const { return playing_; }
    bool isWaitingInput() const { return waiting_; }
    uint8_t size() const { return uint8_t(count_); }
    uint8_t capacity() const { return uint8_t(Capacity); }
    bool overflowed() const { return overflow_; }
    const Node *currentNode() const { return playing_ && count_ ? &buffer_[head_] : nullptr; }
    uint32_t currentElapsedMs() const { return playing_ ? uint32_t(now() - started_) : 0; }
    uint8_t currentFrameIndex(uint16_t delay) const {
        return !delay || !playing_ ? 0 : uint8_t(currentElapsedMs() / delay);
    }
    void update(uint32_t timestamp) {
        if (!playing_ || !count_ || waiting_)
            return;
        const auto duration = Traits::duration(buffer_[head_]);
        if (duration && uint32_t(timestamp - started_) >= duration)
            advance(timestamp);
    }
    void resumeFromInput() {
        if (waiting_) {
            waiting_ = false;
            advance(now());
        }
    }
    void skipCurrent() {
        if (playing_ && count_ && !waiting_)
            advance(now());
    }
    void setOnQueueComplete(QueueCompleteCallback cb) { complete_ = cb; }
    void setOnNodeChange(NodeChangeCallback cb) { change_ = cb; }
};
} // namespace engine
