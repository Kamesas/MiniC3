// main.cpp
//
// Wires the three modules together:
//   - Eyes              : the OLED face / temperature display
//   - Button            : the press-to-show-temperature button
//   - TemperatureSensor : the DHT11 sensor
//
// In Arduino-style sketches:
//   setup() runs once at boot
//   loop()  runs over and over forever
//
// NOTE: when the build flag -DDISPLAY_TEST is set in platformio.ini,
// this file is skipped and src/display_test.cpp takes over instead.

#ifndef DISPLAY_TEST

#include <Arduino.h>
#include "Eyes.h"
#include "Button.h"
#include "TemperatureSensor.h"

// Pin assignments for the LOLIN C3 Mini.
// "constexpr" means "constant known at compile time". It's the modern
// C++ replacement for `#define` for simple values like these.
constexpr uint8_t DHT_PIN    = 1;
// GPIO 3 - safe pin for an external button on the C3 SuperMini.
// (We avoid GPIO 9 here: it's the BOOT-mode strap pin, and if it's
// pulled LOW at reset the chip enters download mode and never runs
// the sketch.)
constexpr uint8_t BUTTON_PIN = 3;
constexpr uint8_t OLED_SDA   = 5;
constexpr uint8_t OLED_SCL   = 6;

// How long to keep showing the temperature after the button is pressed.
constexpr unsigned long SHOW_TEMPERATURE_MS = 5000;

// Frame delay - 30 ms gives roughly 33 frames per second.
constexpr unsigned long FRAME_DELAY_MS = 30;

// Create one instance of each module. Because they live outside any
// function they are "global" objects: they exist for the whole life of
// the program and are constructed before setup() runs.
Eyes              eyes(OLED_SDA, OLED_SCL);
Button            button(BUTTON_PIN);
TemperatureSensor sensor(DHT_PIN, DHT11);

// When (in millis since boot) to stop showing the temperature and go
// back to the animated face.
unsigned long showTempUntil = 0;

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("Thermometer with Cozmo eyes started!");

    Serial.println("button.begin()");  Serial.flush();
    button.begin();
    Serial.println("sensor.begin()");  Serial.flush();
    sensor.begin();
    Serial.println("eyes.begin()");    Serial.flush();
    eyes.begin();
    Serial.println("setup done");      Serial.flush();
}

void loop() {
    static unsigned long lastHeartbeat = 0;
    unsigned long now = millis();

    // Print "alive" once per second so we can see if loop() is running.
    if (now - lastHeartbeat > 1000) {
        Serial.println("loop alive");
        lastHeartbeat = now;
    }

    // 1. If the user pressed the button, schedule the temperature view
    //    to stay on screen for SHOW_TEMPERATURE_MS milliseconds.
    if (button.wasPressed()) {
        Serial.println("BUTTON pressed -> temp mode");
        showTempUntil = now + SHOW_TEMPERATURE_MS;
    }

    // 2. Either show the temperature, or animate the eyes.
    if (now < showTempUntil) {
        float temp     = sensor.readTemperature();
        float humidity = sensor.readHumidity();
        Serial.printf("temp branch: t=%.1f h=%.1f\n", temp, humidity);

        if (!isnan(temp) && !isnan(humidity)) {
            eyes.showTemperature(temp, humidity);
            // Pre-set the mood so when we return to the face, it
            // reflects the temperature we just measured.
            eyes.setMoodFromTemperature(temp);
        } else {
            // DHT didn't respond - draw something so the user can see
            // the button worked even without a working sensor.
            eyes.showTemperature(-99.9f, 0.0f);
        }
    } else {
        eyes.update();
    }

    delay(FRAME_DELAY_MS);
}

#endif  // !DISPLAY_TEST
