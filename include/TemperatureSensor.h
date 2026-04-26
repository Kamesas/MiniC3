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
    float readTemperature();

    // Read the current relative humidity in percent. Can also return NaN.
    float readHumidity();

private:
    DHT dht;
};
