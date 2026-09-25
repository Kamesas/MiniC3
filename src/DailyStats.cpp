// DailyStats.cpp

#include "DailyStats.h"
#include <math.h>

DailyStats::DailyStats()
    : tempMin_(NAN), tempMax_(NAN),
      humidityMin_(NAN), humidityMax_(NAN),
      currentDay_(-1) {}

void DailyStats::recordSample(float temp, float humidity, int yday) {
    resetIfNewDay(yday);

    // Guard against NaN: a NaN sample shouldn't pollute the stored
    // min/max. NaN compares as false against everything, so we have
    // to check explicitly with isnan() before using the value.
    if (!isnan(temp)) {
        if (isnan(tempMin_) || temp < tempMin_) tempMin_ = temp;
        if (isnan(tempMax_) || temp > tempMax_) tempMax_ = temp;
    }
    if (!isnan(humidity)) {
        if (isnan(humidityMin_) || humidity < humidityMin_) humidityMin_ = humidity;
        if (isnan(humidityMax_) || humidity > humidityMax_) humidityMax_ = humidity;
    }
}

// Reset all four stats when the day-of-year changes. The very first
// call (currentDay_ == -1) also reaches this branch, which is correct -
// it locks in "today" as whatever date we just learned.
void DailyStats::resetIfNewDay(int yday) {
    if (currentDay_ != yday) {
        tempMin_     = NAN;
        tempMax_     = NAN;
        humidityMin_ = NAN;
        humidityMax_ = NAN;
        currentDay_  = yday;
    }
}
