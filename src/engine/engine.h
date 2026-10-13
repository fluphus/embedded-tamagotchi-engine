#pragma once
#include "contract.h"
namespace engine {
inline constexpr uint32_t MaxAdvanceMinutes=1440;
// Policy is a statically selected, typed C++ strategy. No platform/content headers.
template <class Policy> class Engine {
    Ports ports_;
    bool commandRejected_=false;
    Queue<Event, 16> events_;
    uint32_t lastSave_ = 0;
    bool saveMarked_ = false;
    void emit(EventKind kind, const State &state, EpochSeconds time) {
        events_.push({kind, state.form, state.health, state.seriousness, time});
    }

  public:
    explicit Engine(Ports ports = {}) : ports_(ports) {}
    void setPorts(Ports ports) { ports_ = ports; }
    const Ports &ports() const { return ports_; }
    using Result = typename Policy::Result;
    bool commandRejected() const {return commandRejected_;}
    Result execute(State &state, Command command) {
        commandRejected_=command.kind==CommandKind::AdvanceMinutes&&command.minutes>MaxAdvanceMinutes;
        if(commandRejected_){if(ports_.diagnostic)ports_.diagnostic(ports_.context,"[Engine] REJECTED: minute batch exceeds 1440");return Result{};}
        const State before = state; // command boundary only, never a per-frame operation
        Result result{};
        if (command.kind == CommandKind::AdvanceMinutes) {
            const uint32_t end = command.time.value;
            uint32_t cursor = end - command.minutes * 60u; // legacy calendar arithmetic
            for (uint32_t i = 0; i < command.minutes && !Policy::terminal(state); ++i) {
                cursor += 60;
                Command minute;
                minute.kind = CommandKind::Minute;
                minute.time = {cursor};
                result = Policy::handle(state, minute, ports_);
            }
            Policy::summarize(before, state, command, result);
        } else
            result = Policy::handle(state, command, ports_);
        emit(EventKind::CommandApplied, state, command.time);
        if (before.form != state.form)
            emit(EventKind::FormChanged, state, command.time);
        if (before.health != state.health || before.seriousness != state.seriousness)
            emit(EventKind::MetricChanged, state, command.time);
        return result;
    }
    Result executeNow(State &state, Command command) {
        if (ports_.calendar)
            command.time = ports_.calendar(ports_.context);
        return execute(state, command);
    }
    bool poll(Event &event) { return events_.pop(event); }
    bool eventOverflowed() const { return events_.overflowed(); }
    void discardEvents() { events_.clear(); }
    bool saveDue(uint32_t intervalSeconds = 300) const {
        if (!ports_.monotonic)
            return false;
        return !saveMarked_ || uint32_t(ports_.monotonic(ports_.context).value - lastSave_) >=
                                   intervalSeconds * 1000u;
    }
    void markSaved() {
        if (ports_.monotonic) {
            lastSave_ = ports_.monotonic(ports_.context).value;
            saveMarked_ = true;
        }
    }
};
} // namespace engine
