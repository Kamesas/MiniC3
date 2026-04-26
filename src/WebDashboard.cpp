// WebDashboard.cpp

#include "WebDashboard.h"
#include "TemperatureSensor.h"
#include "TemperatureHistory.h"
#include "Eyes.h"

#include <ESPmDNS.h>
#include <math.h>

// The whole HTML page. PROGMEM puts the string in flash instead of
// RAM (~2 KB saved) - we read it back with the F() macro when sending.
// Single-page app: HTML structure + inline CSS + inline JS that polls
// /data every 5 seconds and redraws an SVG line chart.
static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>MiniC3</title>
<style>
  :root { color-scheme: dark; }
  body {
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    max-width: 480px; margin: 0 auto; padding: 1em;
    background: #111; color: #eee;
  }
  h1 { font-size: 1.1em; color: #888; font-weight: 500; margin: 1em 0 0.5em; }
  .card {
    background: #1f1f1f; border-radius: 14px;
    padding: 1.2em 1.4em; margin-bottom: 0.8em;
  }
  .row { display: flex; justify-content: space-between; align-items: baseline; }
  .big { font-size: 2.6em; font-weight: 200; line-height: 1; }
  .label { color: #888; font-size: 0.75em; text-transform: uppercase; letter-spacing: 0.05em; }
  .meta { display: grid; grid-template-columns: repeat(3, 1fr); gap: 0.5em 1em; }
  .meta div .v { font-size: 1.1em; font-weight: 500; margin-top: 0.2em; }
  svg { width: 100%; height: 110px; display: block; margin-top: 0.6em; }
  .axis { stroke: #333; stroke-width: 1; }
  .line { fill: none; stroke: #4a9eff; stroke-width: 1.6; }
  .area { fill: rgba(74,158,255,0.12); stroke: none; }
  .stale { color: #f55; }
</style>
</head>
<body>
<h1>MiniC3</h1>
<div class="card">
  <div class="row">
    <div>
      <div class="label">Temperature</div>
      <div class="big" id="temp">&mdash;</div>
    </div>
    <div style="text-align:right">
      <div class="label">Humidity</div>
      <div class="big" id="hum">&mdash;</div>
    </div>
  </div>
</div>
<div class="card">
  <div class="label">Last hour &mdash; temperature</div>
  <svg id="chart" viewBox="0 0 60 100" preserveAspectRatio="none">
    <line class="axis" x1="0" y1="50" x2="60" y2="50"/>
    <path class="area" id="area"/>
    <polyline class="line" id="line"/>
  </svg>
</div>
<div class="card meta">
  <div><div class="label">Time</div><div class="v" id="time">&mdash;</div></div>
  <div><div class="label">Mood</div><div class="v" id="mood">&mdash;</div></div>
  <div><div class="label">Uptime</div><div class="v" id="uptime">&mdash;</div></div>
</div>
<script>
const $ = id => document.getElementById(id);
function fmtUptime(s) {
  if (s < 60) return s + "s";
  if (s < 3600) return Math.floor(s/60) + "m";
  return Math.floor(s/3600) + "h " + Math.floor((s%3600)/60) + "m";
}
async function refresh() {
  let d;
  try { d = await (await fetch('/data')).json(); }
  catch (e) { document.title = "MiniC3 (offline)"; return; }
  document.title = "MiniC3";
  $('temp').textContent = d.temp == null ? "—" : d.temp.toFixed(1) + "°";
  $('hum').textContent  = d.humidity == null ? "—" : d.humidity.toFixed(0) + "%";
  $('time').textContent = d.time || "—";
  $('mood').textContent = d.mood || "—";
  $('uptime').textContent = fmtUptime(d.uptime_s);
  // Chart: pad the array on the LEFT with nulls so the newest sample is at x=59
  const N = 60, h = d.history;
  const padded = new Array(N - h.length).fill(null).concat(h);
  const valid = padded.filter(v => v != null);
  if (valid.length < 2) { $('line').setAttribute('points', ''); $('area').setAttribute('d', ''); return; }
  const lo = Math.min(...valid) - 0.3, hi = Math.max(...valid) + 0.3, range = hi - lo || 1;
  const pts = [];
  padded.forEach((v, i) => { if (v != null) pts.push(i + ',' + (100 - (v - lo) / range * 100)); });
  $('line').setAttribute('points', pts.join(' '));
  // Area underneath the line for a nicer look
  if (pts.length) {
    const first = pts[0].split(',')[0], last = pts[pts.length-1].split(',')[0];
    $('area').setAttribute('d', 'M' + first + ',100 L' + pts.join(' L') + ' L' + last + ',100 Z');
  }
}
refresh();
setInterval(refresh, 5000);
</script>
</body>
</html>)HTML";

WebDashboard::WebDashboard(TemperatureSensor& sensor,
                           TemperatureHistory& history,
                           Eyes& eyes)
    : sensor_(sensor), history_(history), eyes_(eyes), server_(80) {}

void WebDashboard::begin(const char* hostname) {
    if (!MDNS.begin(hostname)) {
        Serial.println("mDNS: start FAILED (you can still use the IP address)");
    } else {
        Serial.printf("mDNS: ready at http://%s.local/\n", hostname);
    }

    // Lambdas with [this] capture so the handler bodies can call our
    // private member functions. This is the standard Arduino WebServer
    // routing pattern: server.on("/path", callback).
    server_.on("/",     [this]() { handleRoot(); });
    server_.on("/data", [this]() { handleData(); });
    server_.onNotFound([this]() { server_.send(404, "text/plain", "Not found"); });
    server_.begin();
    MDNS.addService("http", "tcp", 80);
}

void WebDashboard::handleClient() {
    server_.handleClient();
}

void WebDashboard::setClock(const char* hhmm) {
    currentClock_ = hhmm;
}

void WebDashboard::handleRoot() {
    // FPSTR streams the PROGMEM string straight from flash without
    // copying the whole thing into RAM first.
    server_.send_P(200, "text/html", INDEX_HTML);
}

// Build the JSON payload manually. Pulling in ArduinoJson would cost
// extra flash for a doc this small and predictable. We use a String
// reserved up-front to avoid repeated reallocations as we append.
void WebDashboard::handleData() {
    String json;
    json.reserve(900);
    json = '{';

    float t = sensor_.readTemperature();
    float h = sensor_.readHumidity();

    auto appendNumberOrNull = [&json](float v, int decimals) {
        if (isnan(v)) { json += "null"; return; }
        json += String(v, decimals);
    };

    json += "\"temp\":";
    appendNumberOrNull(t, 1);
    json += ",\"humidity\":";
    appendNumberOrNull(h, 1);
    json += ",\"time\":\"";
    json += currentClock_;  // empty string if NTP not ready - JS handles "—"
    json += "\",\"mood\":\"";
    json += eyes_.getMoodName();
    json += "\",\"uptime_s\":";
    json += String(millis() / 1000UL);
    json += ",\"history\":[";

    // Only include real samples (count() of them, oldest first). The
    // browser knows to right-align the array onto the chart's 60 slots.
    for (size_t i = 0; i < history_.count(); i++) {
        if (i > 0) json += ',';
        appendNumberOrNull(history_.tempAt(i), 1);
    }
    json += "]}";

    // CORS isn't needed (same-origin) but no-cache stops phones from
    // showing stale numbers if the user reloads quickly.
    server_.sendHeader("Cache-Control", "no-cache");
    server_.send(200, "application/json", json);
}
