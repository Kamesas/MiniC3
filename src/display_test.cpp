// display_test.cpp
//
// Minimal "is the OLED alive?" sketch. It does NOTHING with the button
// or the DHT sensor - only the display. Use it to debug wiring/soldering.
//
// HOW TO ENABLE:
//   In platformio.ini, set the build flag:  -DDISPLAY_TEST
//   Then build & upload. Switch back by removing the flag.
//
// Behavior:
//   1. Scans the I2C bus and prints any device addresses it finds to
//      Serial (115200 baud). A working SSD1306 normally answers at 0x3C
//      (sometimes 0x3D). If nothing shows up, it's a wiring problem.
//   2. Tries to talk to the OLED and draw a counter that ticks every
//      half second.

#ifdef DISPLAY_TEST

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

// Same pins as the real project: SDA=5, SCL=6.
constexpr uint8_t OLED_SDA = 5;
constexpr uint8_t OLED_SCL = 6;

// SOFTWARE I2C variant - U8g2 toggles the pins itself instead of using
// the Wire library. This is the known-good workaround for the "begin()
// hangs on ESP32-C3" issue we hit with the HW_I2C variant.
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, OLED_SCL, OLED_SDA,
                                         U8X8_PIN_NONE);

// Sweeps every possible 7-bit I2C address and lists the ones that ACK.
void scanI2C() {
    Serial.println("Scanning I2C bus...");
    int found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("  found device at 0x%02X\n", addr);
            found++;
        }
    }
    if (found == 0) {
        Serial.println("  NO devices found - check wiring/power/pull-ups.");
    } else {
        Serial.printf("Done. %d device(s) found.\n", found);
    }
}

void setup() {
    Serial.begin(115200);

    // ESP32-C3 talks USB-CDC to the host. After boot the host needs a
    // moment to re-enumerate the new serial port; printing too early
    // means the bytes go nowhere. Wait up to 3 seconds for the host to
    // open the port, then push a few "alive" lines either way.
    unsigned long start = millis();
    while (!Serial && millis() - start < 3000) {
        delay(50);
    }
    for (int i = 0; i < 5; i++) {
        Serial.printf("[boot] alive %d\n", i);
        Serial.flush();
        delay(200);
    }

    Serial.println("\n--- Display test ---");

    // Bring up I2C on the same pins for the scan only. (U8g2 SW_I2C
    // doesn't use Wire at all, so this is just a sanity check.)
    Wire.begin(OLED_SDA, OLED_SCL);
    Wire.setClock(100000);
    Wire.setTimeOut(1000);
    scanI2C();
    Wire.end();   // release the bus so SW_I2C can take over the pins

    Serial.println("calling u8g2.begin() (SW_I2C)...");
    Serial.flush();
    bool ok = u8g2.begin();
    Serial.printf("u8g2.begin() returned %d\n", ok);

    u8g2.setContrast(255);
    u8g2.setPowerSave(0);

    Serial.println("u8g2 ready");
}

void loop() {
    static int counter = 0;

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB14_tr);
    u8g2.drawStr(0, 20, "Display OK");
    u8g2.drawFrame(0, 0, 128, 64);

    char buf[16];
    snprintf(buf, sizeof(buf), "count: %d", counter++);
    u8g2.drawStr(0, 50, buf);

    u8g2.sendBuffer();
    Serial.printf("frame %d\n", counter);
    delay(500);
}

#endif  // DISPLAY_TEST
