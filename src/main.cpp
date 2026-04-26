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
#include <WiFi.h>     // ESP32 WiFi client (built in to the framework)
#include <time.h>     // configTzTime, getLocalTime, struct tm
#include "Eyes.h"
#include "Button.h"
#include "TemperatureSensor.h"
#include "TemperatureHistory.h"
#include "WebDashboard.h"
#include "secrets.h"  // WIFI_SSID / WIFI_PASSWORD / TIMEZONE

// Hostname used for both mDNS (http://miniC3.local/) and the WiFi
// station name visible in the router's client list.
static constexpr const char* HOSTNAME = "miniC3";

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

// Turn the OLED off after this many milliseconds with no button press.
// 2 minutes feels like a reasonable "nobody's looking" timeout - extends
// the panel's life without being annoying.
constexpr unsigned long IDLE_SLEEP_MS = 2UL * 60UL * 1000UL;

// While the display is awake and showing the eyes, refresh the cached
// temperature reading this often. Keeps the next button-press snappy
// (no "loading..." flash) without hammering the DHT11.
constexpr unsigned long BACKGROUND_POLL_MS = 30UL * 1000UL;

// How long to keep trying WiFi at boot before giving up. We'd rather
// have a working thermometer with no clock than a device that hangs
// in setup() because the router is off.
constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 10UL * 1000UL;

// How often to refresh the displayed clock string. Once a second is
// plenty - the clock only shows HH:MM, no seconds.
constexpr unsigned long CLOCK_REFRESH_MS = 1000UL;

// How often to record a sample to the temperature history ring buffer.
// 60 seconds * 60 slots = exactly one hour of history.
constexpr unsigned long HISTORY_SAMPLE_MS = 60UL * 1000UL;

// Create one instance of each module. Because they live outside any
// function they are "global" objects: they exist for the whole life of
// the program and are constructed before setup() runs.
Eyes               eyes(OLED_SDA, OLED_SCL);
Button             button(BUTTON_PIN);
TemperatureSensor  sensor(DHT_PIN, DHT11);
TemperatureHistory history;
WebDashboard       dashboard(sensor, history, eyes);

// When (in millis since boot) to stop showing the temperature and go
// back to the animated face.
unsigned long showTempUntil = 0;

// Time of the most recent button press. Used to decide when to put the
// display to sleep. Initialized in setup() to "now" so the device is
// awake for the first IDLE_SLEEP_MS after boot.
unsigned long lastInteractionMs = 0;

// True once the display has been put to sleep due to inactivity.
bool displayAsleep = false;

// When the background DHT poll last ran (only ticks while awake).
unsigned long lastBackgroundPollMs = 0;

// True once WiFi connected at boot. If false we never try to read the
// clock (the system time would just be 1970 and the display would be
// confusing). WiFi failure isn't fatal - the rest of the device works.
bool wifiOk = false;

// When we last refreshed the clock string handed to Eyes.
unsigned long lastClockRefreshMs = 0;

// When we last recorded a sample into the history ring buffer.
unsigned long lastHistorySampleMs = 0;

// Try to connect to WiFi, returning true on success. Blocks for up to
// WIFI_CONNECT_TIMEOUT_MS - if the router is off or the credentials
// are wrong we give up and run without a clock instead of hanging.
static bool connectWifi() {
    Serial.printf("WiFi: connecting to %s ...\n", WIFI_SSID);
    Serial.flush();

    WiFi.mode(WIFI_STA);                       // STA = station = client mode
    WiFi.setHostname(HOSTNAME);                // shows up in router DHCP list
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
        delay(250);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi: connect FAILED, continuing without clock");
        return false;
    }
    Serial.print("WiFi: connected, IP = ");
    Serial.println(WiFi.localIP());
    return true;
}

// Configure the system clock to sync from NTP servers, applying our
// timezone. The ESP32's lwIP SNTP client runs in the background -
// the first sync usually completes within a few seconds of WiFi up.
static void startNtp() {
    // 1st arg = POSIX TZ string (DST rules, offsets); 2nd/3rd = NTP servers.
    configTzTime(TIMEZONE, "pool.ntp.org", "time.nist.gov");
    Serial.println("NTP: sync started");
}

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

    // WiFi + NTP. Both are best-effort - on failure the device still
    // runs as a normal thermometer, just without a clock display.
    wifiOk = connectWifi();
    if (wifiOk) {
        startNtp();
        dashboard.begin(HOSTNAME);
    }

    lastInteractionMs = millis();      // start the idle countdown now
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

    // Service the web dashboard. Cheap if no client is connected.
    if (wifiOk) {
        dashboard.handleClient();
    }

    // Refresh the clock text shown above the eyes. Cheap operation -
    // just formats the system clock into "HH:MM" and hands it to Eyes
    // (and the web dashboard). Skipped entirely if WiFi never connected.
    if (wifiOk && (now - lastClockRefreshMs > CLOCK_REFRESH_MS)) {
        struct tm timeinfo;
        // getLocalTime() returns false until the first NTP sync lands.
        // Pass timeout=0 so it never blocks - we'll just try again next tick.
        if (getLocalTime(&timeinfo, 0)) {
            char hhmm[6];
            snprintf(hhmm, sizeof(hhmm), "%02d:%02d",
                     timeinfo.tm_hour, timeinfo.tm_min);
            eyes.setClock(hhmm);
            dashboard.setClock(hhmm);
        }
        lastClockRefreshMs = now;
    }

    // Append a fresh sample to the history ring buffer once a minute.
    // sensor.poll() does the actual DHT read; readTemperature() then
    // returns the cached value with no extra I/O. This keeps the chart
    // on the dashboard moving even when nobody is pressing the button.
    if (now - lastHistorySampleMs > HISTORY_SAMPLE_MS) {
        sensor.poll();
        history.recordSample(sensor.readTemperature(), sensor.readHumidity());
        lastHistorySampleMs = now;
    }

    // 1. If the user pressed the button, mark the interaction (resets
    //    the idle timer), wake the display if it had gone to sleep,
    //    and schedule the temperature view for SHOW_TEMPERATURE_MS.
    if (button.wasPressed()) {
        Serial.println("BUTTON pressed -> temp mode");
        lastInteractionMs = now;
        if (displayAsleep) {
            eyes.wake();
            displayAsleep = false;
        }
        showTempUntil = now + SHOW_TEMPERATURE_MS;
    }

    // 2. Drive the display in one of three states:
    //    a) Showing the temperature  (button was pressed within the
    //       last SHOW_TEMPERATURE_MS).
    //    b) Asleep                   (no presses for IDLE_SLEEP_MS).
    //    c) Animating the eyes       (default idle behavior).
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
    } else if (!displayAsleep && (now - lastInteractionMs) > IDLE_SLEEP_MS) {
        // Idle long enough - put the panel to sleep until the next press.
        Serial.println("idle timeout -> sleep");
        eyes.sleep();
        displayAsleep = true;
    } else if (!displayAsleep) {
        // Animating. Do a low-frequency background DHT poll so the next
        // button press has a fresh value already cached. We deliberately
        // SKIP this when asleep: nobody's looking, give the sensor a rest.
        if (now - lastBackgroundPollMs > BACKGROUND_POLL_MS) {
            sensor.poll();
            lastBackgroundPollMs = now;
        }
        eyes.update();
    }
    // (else: asleep and no press - just spin, doing nothing, until
    //  wasPressed() becomes true above.)

    delay(FRAME_DELAY_MS);
}

#endif  // !DISPLAY_TEST
