#pragma once
#include "../contracts/feeding_types.h"
class ArtoriaFeedingSystem {
public:

    FeedOutcome feed(PetState& pet, const uint8_t picked[3],
                     uint32_t currentTime, uint8_t currentHour);
    void applySpecialFood(PetState& pet, FeedOutcome& outcome, uint8_t specialFoodId);
    DayEndOutcome processDayEnd(PetState& pet);
    void resetDaily(PetState& pet, uint8_t newDay);

    FeedResult canFeed(const PetState& pet, uint32_t currentTime);
    uint32_t secondsUntilNextFeed(const PetState& pet, uint32_t currentTime);
    bool isInFeedWindow(uint8_t hour);
    int8_t getWindowIndex(uint8_t hour);

private:
    int16_t clampHealth(int16_t value);
    ComboType checkCombo(const uint8_t picked[3]);
    int16_t calcDislikeSeriousness(const PetState& pet, const uint8_t picked[3]);
    int16_t calcComboSeriousness(const PetState& pet, ComboType combo);
    int16_t calcComboHealth(const PetState& pet, ComboType combo);
    bool rollMapoTofu();
};

