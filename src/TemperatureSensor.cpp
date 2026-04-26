// TemperatureSensor.cpp

#include "TemperatureSensor.h"

// The DHT library's class also has no default constructor, so we have
// to build it in the initializer list with the pin and sensor type.
// We start the cache as NaN so callers see "no reading yet" until the
// first real read completes.
TemperatureSensor::TemperatureSensor(uint8_t pin, uint8_t type)
    : dht(pin, type),
      cachedTemp(NAN),
      cachedHumidity(NAN),
      lastReadMs(0) {}

void TemperatureSensor::begin() {
    dht.begin();
}

float TemperatureSensor::readTemperature() {
    refreshIfStale();
    return cachedTemp;
}

float TemperatureSensor::readHumidity() {
    refreshIfStale();
    return cachedHumidity;
}

void TemperatureSensor::poll() {
    refreshIfStale();
}

// Only hits the actual DHT11 if the cache is older than the minimum
// interval. A real read takes ~20 ms of blocking I/O, so we want to
// avoid doing it on every animation frame.
void TemperatureSensor::refreshIfStale() {
    unsigned long now = millis();
    bool neverRead = (lastReadMs == 0);
    if (neverRead || (now - lastReadMs) >= MIN_READ_INTERVAL_MS) {
        cachedTemp     = dht.readTemperature();
        cachedHumidity = dht.readHumidity();
        lastReadMs     = now;
    }
}
