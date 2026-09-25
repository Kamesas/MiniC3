// DailyStats.h
//
// Tracks today's high and low temperature + humidity. Resets
// automatically at local midnight - the caller hands in the current
// day-of-year (from struct tm.tm_yday) and we notice when it changes.

#pragma once

#include <Arduino.h>

class DailyStats {
public:
    DailyStats();

    // Update min/max with a fresh sample. If `yday` differs from the
    // last call, the stats reset first (a new day has started).
    // NaN inputs are silently ignored - useful when the DHT failed.
    void recordSample(float temp, float humidity, int yday);

    // Read accessors. Return NaN until at least one valid sample has
    // arrived for the current day.
    float tempMin()     const { return tempMin_; }
    float tempMax()     const { return tempMax_; }
    float humidityMin() const { return humidityMin_; }
    float humidityMax() const { return humidityMax_; }

private:
    void resetIfNewDay(int yday);

    float tempMin_, tempMax_;
    float humidityMin_, humidityMax_;
    int   currentDay_;  // tm_yday value; -1 = no day captured yet
};
