// TemperatureSensor.h
//
// Wrapper around the DHT11 temperature/humidity sensor. The underlying
// Adafruit DHT library works fine on its own - this wrapper just gives
// the rest of the code a small, focused interface to talk to.

#pragma once

#include <Arduino.h>
#include <DHT.h>

class TemperatureSensor {
public:
    // pin  = the data pin the sensor is wired to.
    // type = the sensor model: DHT11, DHT22, or DHT21 (constants from
    //        the DHT library).
    TemperatureSensor(uint8_t pin, uint8_t type);

    // Initialize the sensor. Call once from setup().
    void begin();

    // Read the current temperature in Celsius. Can return NaN ("not a
    // number") if the sensor failed to respond - check with isnan()
    // before using the value.
    //
    // Internally rate-limited: the DHT11 datasheet says no more than one
    // reading per second, and a real read is slow (~20 ms blocking).
    // Calls within MIN_READ_INTERVAL_MS of the last real read just
    // return the cached value, so it's safe to call this every frame.
    float readTemperature();

    // Read the current relative humidity in percent. Can also return NaN.
    // Same rate-limiting and caching as readTemperature().
    float readHumidity();

    // Force a fresh read if the cache is older than MIN_READ_INTERVAL_MS.
    // Useful for "background" polling so the next display update has a
    // recent value already cached. No-op if the cache is still fresh.
    void poll();

private:
    // The minimum gap between two real sensor reads. The DHT11 can't be
    // polled faster than 1 Hz; 2000 ms gives the sensor headroom.
    static constexpr unsigned long MIN_READ_INTERVAL_MS = 2000;

    // Triggers a real sensor read if enough time has passed; otherwise
    // leaves the cached values alone.
    void refreshIfStale();

    DHT dht;
    float cachedTemp;
    float cachedHumidity;
    unsigned long lastReadMs;  // 0 = never read yet
};
