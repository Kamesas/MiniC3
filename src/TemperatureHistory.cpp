// TemperatureHistory.cpp

#include "TemperatureHistory.h"

TemperatureHistory::TemperatureHistory() : head(0), filled(0) {
    // Pre-fill arrays with NaN so any accidental read of an unused
    // slot returns "no data" instead of garbage memory.
    for (size_t i = 0; i < SIZE; i++) {
        temps[i] = NAN;
        humidities[i] = NAN;
    }
}

void TemperatureHistory::recordSample(float temp, float humidity) {
    temps[head] = temp;
    humidities[head] = humidity;
    head = (head + 1) % SIZE;
    if (filled < SIZE) {
        filled++;
    }
    // (When filled == SIZE, head wrapping around naturally overwrites
    //  the oldest sample on the next write.)
}

// Translate "i-th oldest" into a real array index. While the buffer
// is still filling, the oldest sample is at index 0. Once full, the
// oldest sample is at `head` (the next-write slot is the oldest one).
float TemperatureHistory::tempAt(size_t i) const {
    if (i >= filled) return NAN;
    size_t start = (filled < SIZE) ? 0 : head;
    return temps[(start + i) % SIZE];
}

float TemperatureHistory::humidityAt(size_t i) const {
    if (i >= filled) return NAN;
    size_t start = (filled < SIZE) ? 0 : head;
    return humidities[(start + i) % SIZE];
}
