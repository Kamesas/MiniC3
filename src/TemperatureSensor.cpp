// TemperatureSensor.cpp

#include "TemperatureSensor.h"

// The DHT library's class also has no default constructor, so we have
// to build it in the initializer list with the pin and sensor type.
TemperatureSensor::TemperatureSensor(uint8_t pin, uint8_t type)
    : dht(pin, type) {}

void TemperatureSensor::begin() {
    dht.begin();
}

float TemperatureSensor::readTemperature() {
    return dht.readTemperature();
}

float TemperatureSensor::readHumidity() {
    return dht.readHumidity();
}
