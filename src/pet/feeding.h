#pragma once
#include "../content/contracts/feeding_types.h"
class FeedingSystem {
public:
    FeedDraw drawFood();
    FeedOutcome feed(PetState& pet, const uint8_t picked[3],
                     uint32_t currentTime, uint8_t currentHour);
    void applySpecialFood(PetState& pet, FeedOutcome& outcome, uint8_t specialFoodId);
    DayEndOutcome processDayEnd(PetState& pet);
    void resetDaily(PetState& pet, uint8_t newDay);

    FeedResult canFeed(const PetState& pet, uint32_t currentTime);
    uint32_t secondsUntilNextFeed(const PetState& pet, uint32_t currentTime);
    bool isInFeedWindow(uint8_t hour);
    int8_t getWindowIndex(uint8_t hour);

};

extern FeedingSystem feedingSystem;
