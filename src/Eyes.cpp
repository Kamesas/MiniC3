// Eyes.cpp
// Implementation of everything declared in include/Eyes.h.

#include "Eyes.h"
#include <Arduino.h>  // for millis() and random()

// Constructor body. The bit between ":" and "{" is called an
// "initializer list" - it constructs each member with the given value
// before the body runs. We use it here because the U8G2 display object
// has no default constructor: we must hand it the pins up front.
// Note the SW_I2C constructor takes pins in a different order than
// HW_I2C: (rotation, clock, data, reset).
Eyes::Eyes(uint8_t sdaPin, uint8_t sclPin)
    : u8g2(U8G2_R0, sclPin, sdaPin, U8X8_PIN_NONE),
      nextBlink(0),
      blinkUntil(0),
      nextLook(0),
      isBlinking(false),
      lookX(0), lookY(0),
      targetLookX(0), targetLookY(0),
      currentMood(Normal),
      leftEye{36, 36, 8, 0, 0},
      rightEye{36, 36, 8, 0, 0},
      batteryPercent(-1) {
    clockText[0] = '\0';  // start hidden until main.cpp sets a time
}

void Eyes::begin() {
    u8g2.begin();
    u8g2.setContrast(255);    // max brightness
    u8g2.setPowerSave(0);     // make sure the panel isn't in sleep mode
    u8g2.setFont(u8g2_font_ncenB08_tr);


    // Schedule the first random animation events a couple of seconds out.
    nextBlink = millis() + random(2000, 4000);
    nextLook  = millis() + random(1000, 3000);
}

void Eyes::setMood(Mood mood) {
    currentMood = mood;
    switch (mood) {
        case Normal:
            leftEye  = {36, 36, 8, 0, 0};
            rightEye = {36, 36, 8, 0, 0};
            break;
        case Happy:  // squinted
            leftEye  = {36, 28, 8, 0.3f, 0.2f};
            rightEye = {36, 28, 8, 0.3f, 0.2f};
            break;
        case Sleepy:
            leftEye  = {36, 20, 8, 0.5f, 0};
            rightEye = {36, 20, 8, 0.5f, 0};
            break;
        case Surprised:  // wide
            leftEye  = {40, 44, 10, 0, 0};
            rightEye = {40, 44, 10, 0, 0};
            break;
    }
}

void Eyes::setMoodFromTemperature(float temp) {
    if (temp > 28) {
        setMood(Sleepy);     // sleepy when hot
    } else if (temp < 20) {
        setMood(Surprised);  // surprised when cold
    } else {
        setMood(Happy);      // happy at a comfortable temperature
    }
}

// Draws one eye. "EyeParams &eye" is a "reference" - it lets us pass
// the eye in without making a copy (faster, and any change inside the
// function would be visible to the caller).
void Eyes::drawEye(int centerX, int centerY, EyeParams &eye,
                   float pupilOffsetX, float pupilOffsetY, bool blink) {
    int w = eye.width;
    int h = blink ? 4 : eye.height;   // "?:" is "if-then-else" as an expression
    int r = blink ? 2 : eye.radius;

    int x = centerX - w / 2;
    int y = centerY - h / 2;

    // Outer rounded rectangle = the eye itself.
    u8g2.drawRBox(x, y, w, h, r);

    if (!blink) {
        // Carve out a pupil by drawing a smaller "off" (black) rectangle.
        int pupilSize = 10;
        int maxOffsetX = (w - pupilSize) / 2 - 4;
        int maxOffsetY = (h - pupilSize) / 2 - 4;

        int pupilX = centerX + (int)(pupilOffsetX * maxOffsetX) - pupilSize / 2;
        int pupilY = centerY + (int)(pupilOffsetY * maxOffsetY) - pupilSize / 2;

        u8g2.setDrawColor(0);  // 0 = "off" pixel, i.e. black hole inside the eye
        u8g2.drawBox(pupilX + 2, pupilY + 2, pupilSize - 4, pupilSize - 4);
        u8g2.setDrawColor(1);  // back to normal "on" drawing

        // Eyelids for moods like Happy/Sleepy: paint a black strip over
        // the top and/or bottom to fake a partially closed eye.
        if (eye.topLidAngle > 0) {
            int lidHeight = (int)(h * eye.topLidAngle);
            u8g2.setDrawColor(0);
            u8g2.drawBox(x, y, w, lidHeight);
            u8g2.setDrawColor(1);
        }
        if (eye.bottomLidAngle > 0) {
            int lidHeight = (int)(h * eye.bottomLidAngle);
            u8g2.setDrawColor(0);
            u8g2.drawBox(x, y + h - lidHeight, w, lidHeight);
            u8g2.setDrawColor(1);
        }
    }
}

void Eyes::setBattery(int percent) {
    batteryPercent = percent;
}

static void drawBatteryIndicator(U8G2& u8g2, int percent) {
    if (percent < 0) return;
    char buf[6];
    snprintf(buf, sizeof(buf), "%d%%", percent);
    int w = u8g2.getStrWidth(buf);
    u8g2.drawStr(127 - w, 7, buf);
}

void Eyes::drawFace() {
    u8g2.clearBuffer();

    // Ease the current look position toward the target so the eyes glide
    // instead of jumping. 0.2 = move 20% of the remaining distance per
    // frame, which gives a nice smooth motion.
    lookX += (targetLookX - lookX) * 0.2f;
    lookY += (targetLookY - lookY) * 0.2f;

    // Left eye centered at x=32, right eye at x=96 on a 128-wide display.
    drawEye(32, 32, leftEye, lookX, lookY, isBlinking);
    drawEye(96, 32, rightEye, lookX, lookY, isBlinking);

    // Clock at the top, between the eyes. The dual-color OLED panel
    // shows pixels in the top 16 rows as yellow and the rest as blue,
    // so the clock will appear in yellow. Only draw when we have a
    // value to show (empty string = WiFi/NTP not ready).
    u8g2.setFont(u8g2_font_5x7_tn);  // 5x7 px digits-and-colon font
    if (clockText[0] != '\0') {
        int w = u8g2.getStrWidth(clockText);
        u8g2.drawStr((128 - w) / 2, 7, clockText);
    }
    drawBatteryIndicator(u8g2, batteryPercent);

    u8g2.sendBuffer();  // push the in-RAM frame buffer to the screen
}

void Eyes::update() {
    static unsigned long frames = 0;
    static unsigned long lastReport = 0;
    frames++;
    unsigned long now = millis();
    if (now - lastReport > 1000) {
        Serial.printf("eyes.update fps=%lu mood=%d look=(%.2f,%.2f) blink=%d\n",
                      frames, currentMood, lookX, lookY, isBlinking);
        frames = 0;
        lastReport = now;
    }

    // Time to start a blink?
    if (now > nextBlink && !isBlinking) {
        isBlinking = true;
        blinkUntil = now + 150;                  // blink lasts 150ms
        nextBlink  = now + random(2000, 5000);   // next blink in 2-5s
    }
    if (now > blinkUntil) {
        isBlinking = false;
    }

    // Time to look somewhere new?
    if (now > nextLook) {
        targetLookX = random(-100, 101) / 100.0f;  // -1.0 .. 1.0
        targetLookY = random(-60, 61) / 100.0f;    // -0.6 .. 0.6
        nextLook    = now + random(800, 2500);
    }

    // Mood is no longer changed at random here - it's driven by the
    // current temperature from main.cpp (see setMoodFromTemperature),
    // so the dashboard's mood legend always tells the truth.

    drawFace();
}

void Eyes::sleep() {
    u8g2.setPowerSave(1);
}

void Eyes::wake() {
    u8g2.setPowerSave(0);
}

// Copy at most sizeof(clockText)-1 characters so we always leave room
// for the null terminator. strncpy + manual null termination is the
// safe idiom in C - strncpy alone won't terminate if the source is
// longer than the destination buffer.
void Eyes::setClock(const char* hhmm) {
    strncpy(clockText, hhmm, sizeof(clockText) - 1);
    clockText[sizeof(clockText) - 1] = '\0';
}

const char* Eyes::getMoodName() const {
    switch (currentMood) {
        case Normal:    return "Normal";
        case Happy:     return "Happy";
        case Sleepy:    return "Sleepy";
        case Surprised: return "Surprised";
    }
    return "?";
}

void Eyes::showTemperature(float temp, float humidity) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_logisoso24_tr);

    // Temperature, e.g. "23.4". snprintf is the safe way to format a
    // float into a fixed-size character array (a C-style string).
    char tempStr[10];
    snprintf(tempStr, sizeof(tempStr), "%.1f", temp);
    int tempWidth = u8g2.getStrWidth(tempStr);
    u8g2.drawStr((128 - tempWidth - 15) / 2, 28, tempStr);

    // "C" with a small circle as the degree symbol.
    u8g2.setFont(u8g2_font_logisoso16_tr);
    u8g2.drawStr((128 + tempWidth - 15) / 2 + 5, 20, "C");
    u8g2.drawCircle((128 + tempWidth - 15) / 2 + 2, 8, 2);

    // Humidity, e.g. "47%".
    u8g2.setFont(u8g2_font_logisoso16_tr);
    char humStr[10];
    snprintf(humStr, sizeof(humStr), "%.0f%%", humidity);
    int humWidth = u8g2.getStrWidth(humStr);
    u8g2.drawStr((128 - humWidth) / 2, 55, humStr);

    u8g2.setFont(u8g2_font_5x7_tn);
    drawBatteryIndicator(u8g2, batteryPercent);

    u8g2.sendBuffer();
}
