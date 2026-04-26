// TemperatureHistory.h
//
// A small fixed-size ring buffer that stores the last hour of
// temperature + humidity samples, one per minute. "Ring buffer" means:
// the buffer is fixed-size and once full, new writes overwrite the
// oldest entries. Perfect for "last hour of data" style displays
// where memory budget is tight.

#pragma once

#include <Arduino.h>

class TemperatureHistory {
public:
    // 60 samples * 1 minute apart = 1 hour of data. Each sample is
    // two floats (8 bytes), so the whole thing costs 480 bytes of RAM.
    static constexpr size_t SIZE = 60;

    TemperatureHistory();

    // Append a new sample. If the buffer is already full the oldest
    // entry is silently dropped to make room. NaN values are stored
    // as-is so the dashboard can show "missing data" gaps.
    void recordSample(float temp, float humidity);

    // How many real samples have been stored (0 .. SIZE).
    size_t count() const { return filled; }

    // Index 0 = oldest stored sample, count()-1 = newest.
    // Returns NaN for out-of-range indices.
    float tempAt(size_t i) const;
    float humidityAt(size_t i) const;

private:
    float temps[SIZE];
    float humidities[SIZE];
    size_t head;     // next slot to write into (modulo SIZE)
    size_t filled;   // grows from 0 to SIZE, then stays at SIZE
};
