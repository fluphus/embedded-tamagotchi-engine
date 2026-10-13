#include "feeding.h"
#include "../app/runtime.h"
#include "../config/game_config.h"
#include "../engine/sampling.h"
FeedingSystem feedingSystem;
FeedDraw FeedingSystem::drawFood() {
    FeedDraw draw{};
    engine::drawDistinct(draw.food_ids, 4, FOOD_COUNT, app::runtime.ports());
    return draw;
}
FeedOutcome FeedingSystem::feed(PetState &p, const uint8_t picked[3], uint32_t now, uint8_t hour) {
    engine::Command c;
    c.kind = engine::CommandKind::Feed;
    c.time = {now};
    c.hour = hour;
    for (size_t i = 0; i < 3; ++i)
        c.foods[i] = picked[i] < FOOD_COUNT ? FOOD_TABLE[picked[i]].id : 0xffff;
    return app::execute(p, c).feed;
}
void FeedingSystem::applySpecialFood(PetState &p, FeedOutcome &outcome, uint8_t id) {
    if (!outcome.combo_triggered || id >= SFOOD_COUNT)
        return;
    engine::Command c;
    c.kind = engine::CommandKind::SpecialFood;
    c.option = SPECIAL_FOOD_TABLE[id].id;
    const auto result = app::execute(p, c).feed;
    outcome.special_food_id = id;
    outcome.mapo_tofu_triggered = result.mapo_tofu_triggered;
    outcome.mapo_tofu_total = result.mapo_tofu_total;
    outcome.mapo_tofu_curse_activated = result.mapo_tofu_curse_activated;
}
DayEndOutcome FeedingSystem::processDayEnd(PetState &p) {
    engine::Command c;
    c.kind = engine::CommandKind::DayEffects;
    return app::execute(p, c).day;
}
void FeedingSystem::resetDaily(PetState &p, uint8_t day) { p.daily_feed.reset(day); }
uint32_t FeedingSystem::secondsUntilNextFeed(const PetState &p, uint32_t now) {
    if (!p.daily_feed.last_feed_time)
        return 0;
    const auto elapsed = now - p.daily_feed.last_feed_time;
    return elapsed >= 1800 ? 0 : 1800 - elapsed;
}
FeedResult FeedingSystem::canFeed(const PetState &p, uint32_t now) {
    if (p.is_rhongomyniad || p.is_black_rhongomyniad)
        return FEED_ERR_RHONGOMYNIAD;
    if (p.daily_feed.feed_count >= 3)
        return FEED_ERR_DAILY_LIMIT;
    return secondsUntilNextFeed(p, now) ? FEED_ERR_TOO_SOON : FEED_OK;
}
bool FeedingSystem::isInFeedWindow(uint8_t h) {
    return (h >= 8 && h < 9) || (h >= 12 && h < 13) || (h >= 18 && h < 19);
}
int8_t FeedingSystem::getWindowIndex(uint8_t h) {
    return h == 8 ? 0 : h == 12 ? 1 : h == 18 ? 2 : -1;
}
