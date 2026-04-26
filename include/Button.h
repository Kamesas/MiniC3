// Button.h
//
// A tiny wrapper around a push button wired between a GPIO pin and
// ground. We use the chip's internal pull-up resistor, so the pin reads
// HIGH when the button is open and LOW when it's pressed.

#pragma once

#include <Arduino.h>

class Button {
public:
    // pin = the GPIO number the button is attached to.
    Button(uint8_t pin);

    // Configure the GPIO. Call once from setup().
    void begin();

    // Returns true exactly ONCE per press: only on the moment the pin
    // transitions from HIGH (released) to LOW (pressed). Holding the
    // button down does NOT keep returning true, and a noisy or stuck
    // LOW pin doesn't flood with fake presses.
    bool wasPressed();

private:
    uint8_t pin;
    bool    pressed;             // current debounced state
    unsigned long lastChangeMs;  // when the pin level last changed
};
