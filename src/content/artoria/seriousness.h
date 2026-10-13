#pragma once
#include "../contracts/seriousness_types.h"
class ArtoriaSeriousnessSystem {
public:
    explicit ArtoriaSeriousnessSystem(SeriousnessLogSink logSink = nullptr) : _logSink(logSink) {}

    // currentTime is calendar epoch SECONDS from the caller, never millis().
    // Tick means one minute supplied by the caller, not elapsed-time detection.
    // Batch ends at currentTime; its start must be representable without underflow.
    // This slice uses no random source. Backward time retains legacy semantics.
    // 待机 tick: 每分钟调用一次, 增加严肃值
    IdleTickResult onIdleTick(PetState& pet, uint32_t currentTime);

    // 批量待机: 快进 N 分钟 (调试用)


    // 互动: 投喂或戳一戳时调用, 降低严肃值
    InteractResult onInteract(PetState& pet, InteractType type, uint32_t currentTime);

    // 未喂满惩罚: 日结算时调用
    void applyMissedFeedPenalty(PetState& pet);

    // 查询当前区间
    SeriousnessTier getTier(int16_t seriousness);
    SeriousnessTier getCurrentTier(const PetState& pet);

    // 查询狮子王计时状态
    RhongoTimerState getRhongoState(const PetState& pet, uint32_t currentTime);
    uint32_t getRhongoElapsed(const PetState& pet, uint32_t currentTime);
    uint32_t getRhongoRemaining(const PetState& pet, uint32_t currentTime);

private:
    SeriousnessLogSink _logSink;
    void log(SeriousnessLogKind kind, int16_t before, int16_t after) const;
    int16_t clamp(int16_t value);
    void updateRhongoTimer(PetState& pet, uint32_t currentTime);
};

