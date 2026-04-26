// Eyes.h
//
// "Header" file. In C++ a header is like a menu of what a module offers:
// it lists the classes and functions other files are allowed to use.
// The actual code that runs lives in src/Eyes.cpp.
//
// Other files write `#include "Eyes.h"` to "import" this menu.

// Include guard. Tells the compiler to skip this file if it has already
// been included once in the current compilation. Without it you get
// "already defined" errors when a header is pulled in from two places.
#pragma once

#include <U8g2lib.h>  // OLED display library

// A "class" is a blueprint for an object. When you create an Eyes
// instance you get one bundle that owns the OLED display and remembers
// the current mood / animation state.
class Eyes {
public:
    // The four faces the robot can wear. An "enum" is just a list of
    // named constants - here Normal == 0, Happy == 1, etc.
    enum Mood { Normal = 0, Happy = 1, Sleepy = 2, Surprised = 3 };

    // Constructor: a special function that runs when an Eyes object is
    // created. The arguments are the ESP32 pins wired to the OLED's
    // I2C bus (SDA = data line, SCL = clock line).
    Eyes(uint8_t sdaPin, uint8_t sclPin);

    // Initialize the display. Call once from setup().
    void begin();

    // Animate one frame of the face: blinking, looking around, random
    // mood changes, and the actual drawing. Call every loop() iteration.
    void update();

    // Replace the face with a temperature + humidity readout.
    void showTemperature(float temp, float humidity);

    // Pick a mood based on the current temperature so that when the face
    // comes back, it reflects how warm/cold it is.
    void setMoodFromTemperature(float temp);

    // Turn the OLED panel off (low-power mode). The display stays dark
    // and the chip stops driving its rows, which extends the panel's
    // lifetime and avoids burn-in. The frame buffer in RAM is preserved.
    void sleep();

    // Turn the OLED panel back on. Pair with sleep().
    void wake();

// Everything below "private:" is internal to the class - other files
// can't touch it. This keeps the public surface small and tidy.
private:
    // Small bundle of numbers describing one eye's shape. A "struct" is
    // just a class with everything public by default - perfect for
    // grouping a few related values.
    struct EyeParams {
        int width;
        int height;
        int radius;
        float topLidAngle;     // 0 = open, 1 = fully closed from the top
        float bottomLidAngle;  // 0 = open, 1 = fully closed from the bottom
    };

    // Helpers used only inside Eyes.cpp.
    void setMood(Mood mood);
    void drawEye(int centerX, int centerY, EyeParams &eye,
                 float pupilOffsetX, float pupilOffsetY, bool blink);
    void drawFace();

    // The OLED driver. We use the SOFTWARE I2C variant (SW_I2C) instead
    // of HW_I2C: U8g2 toggles the SDA/SCL pins itself rather than going
    // through the Wire library. The hardware variant hangs in begin()
    // on this ESP32-C3 + OLED combo, while software I2C works reliably.
    // "F_" = full frame buffer in RAM (needed for smooth animation).
    U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2;

    // Animation timers. All times are in milliseconds since the chip
    // booted (returned by Arduino's millis() function).
    unsigned long nextBlink;
    unsigned long blinkUntil;
    unsigned long nextLook;
    unsigned long nextMood;

    bool isBlinking;

    // Current and target "look direction" of the pupils, in the range
    // -1.0 .. 1.0. Floats so the eyes can drift smoothly between
    // targets instead of snapping in one frame.
    float lookX;
    float lookY;
    float targetLookX;
    float targetLookY;

    // Current mood plus the shape of each eye for that mood.
    Mood currentMood;
    EyeParams leftEye;
    EyeParams rightEye;
};
