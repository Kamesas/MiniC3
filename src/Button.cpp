// Button.cpp

#include "Button.h"

// "pin(pin)" in the initializer list copies the constructor argument
// into the member variable also called "pin".
Button::Button(uint8_t pin) : pin(pin), pressed(false), lastChangeMs(0) {}

void Button::begin() {
    // INPUT_PULLUP enables the chip's internal pull-up resistor so the
    // pin reads HIGH when the button is open and LOW when it's pressed
    // (the button shorts the pin to ground).
    pinMode(pin, INPUT_PULLUP);
}

// Edge-triggered + debounced. We treat a level change as real only if
// the new level has been stable for at least 30 ms - that suppresses
// the contact bounce on cheap tactile switches.
bool Button::wasPressed() {
    bool nowPressed = (digitalRead(pin) == LOW);
    unsigned long now = millis();

    if (nowPressed != pressed && (now - lastChangeMs) > 30) {
        pressed       = nowPressed;
        lastChangeMs  = now;
        // Fire only on the transition INTO the pressed state.
        return nowPressed;
    }
    return false;
}
