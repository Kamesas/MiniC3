// WebDashboard.h
//
// A tiny HTTP server that exposes a single-page dashboard at
//   http://<hostname>.local/
// showing the current temperature, humidity, time, mood, and a small
// SVG line graph of the last hour of temperature readings.
//
// The page is self-contained (HTML + inline CSS + inline JS, no
// external assets) and lives in flash via PROGMEM. The browser polls
// /data every few seconds for fresh values.

#pragma once

#include <WebServer.h>

// Forward declarations - we only need pointers/references in this
// header, so we don't have to pull in the full headers here. This keeps
// compile times fast and reduces unnecessary header coupling.
class TemperatureSensor;
class TemperatureHistory;
class Eyes;

class WebDashboard {
public:
    // The dashboard reads (but does not own) these three modules.
    // References are used instead of pointers because they can never
    // be null and never change after construction.
    WebDashboard(TemperatureSensor& sensor,
                 TemperatureHistory& history,
                 Eyes& eyes);

    // Start the HTTP server and announce the hostname over mDNS so
    // browsers on the LAN can reach us at http://<hostname>.local/.
    // Call once after WiFi has connected.
    void begin(const char* hostname);

    // Service any pending HTTP request. Call from loop() on every tick;
    // it returns immediately if no request is waiting.
    void handleClient();

    // Hand the dashboard the latest "HH:MM" string (same value Eyes
    // gets) so it can include it in /data responses.
    void setClock(const char* hhmm);

private:
    // HTTP request handlers.
    void handleRoot();   // GET /        -> the HTML page
    void handleData();   // GET /data    -> live JSON

    TemperatureSensor&  sensor_;
    TemperatureHistory& history_;
    Eyes&               eyes_;

    WebServer server_;
    String currentClock_;  // "HH:MM" or empty if NTP not yet ready
};
