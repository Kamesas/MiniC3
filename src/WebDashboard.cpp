// WebDashboard.cpp

#include "WebDashboard.h"
#include "TemperatureSensor.h"
#include "TemperatureHistory.h"
#include "DailyStats.h"
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
  .hilo { font-size: 0.85em; color: #aaa; margin-top: 0.4em; }
  .hilo .hi { color: #ff7c7c; margin-right: 0.6em; }
  .hilo .lo { color: #7cb6ff; }
  .meta { display: grid; grid-template-columns: repeat(3, 1fr); gap: 0.5em 1em; }
  .meta div .v { font-size: 1.1em; font-weight: 500; margin-top: 0.2em; }
  svg { width: 100%; height: 90px; display: block; margin-top: 0.6em; }
  .axis { stroke: #333; stroke-width: 1; }
  .lineT { fill: none; stroke: #4a9eff; stroke-width: 1.6; }
  .areaT { fill: rgba(74,158,255,0.12); stroke: none; }
  .lineH { fill: none; stroke: #4ade80; stroke-width: 1.6; }
  .areaH { fill: rgba(74,222,128,0.12); stroke: none; }
  table.legend { width: 100%; border-collapse: collapse; font-size: 0.9em; }
  table.legend th { color: #888; font-weight: 500; text-align: left;
                    padding: 0.3em 0.4em; border-bottom: 1px solid #333; }
  table.legend td { padding: 0.4em; }
  table.legend tr.active td { background: #2c3a4a; color: #fff;
                              border-radius: 6px; font-weight: 500; }
</style>
</head>
<body>
<h1>MiniC3</h1>
<div class="card">
  <div class="row">
    <div>
      <div class="label">Temperature</div>
      <div class="big" id="temp">&mdash;</div>
      <div class="hilo">
        <span class="hi">&uarr; <span id="tMax">&mdash;</span></span>
        <span class="lo">&darr; <span id="tMin">&mdash;</span></span>
      </div>
    </div>
    <div style="text-align:right">
      <div class="label">Humidity</div>
      <div class="big" id="hum">&mdash;</div>
      <div class="hilo">
        <span class="hi">&uarr; <span id="hMax">&mdash;</span></span>
        <span class="lo">&darr; <span id="hMin">&mdash;</span></span>
      </div>
    </div>
  </div>
</div>
<div class="card">
  <div class="label">Last hour &mdash; temperature</div>
  <svg viewBox="0 0 60 100" preserveAspectRatio="none">
    <line class="axis" x1="0" y1="50" x2="60" y2="50"/>
    <path class="areaT" id="areaT"/>
    <polyline class="lineT" id="lineT"/>
  </svg>
</div>
<div class="card">
  <div class="label">Last hour &mdash; humidity</div>
  <svg viewBox="0 0 60 100" preserveAspectRatio="none">
    <line class="axis" x1="0" y1="50" x2="60" y2="50"/>
    <path class="areaH" id="areaH"/>
    <polyline class="lineH" id="lineH"/>
  </svg>
</div>
<div class="card meta">
  <div><div class="label">Time</div><div class="v" id="time">&mdash;</div></div>
  <div><div class="label">Mood</div><div class="v" id="mood">&mdash;</div></div>
  <div><div class="label">Uptime</div><div class="v" id="uptime">&mdash;</div></div>
</div>
<div class="card">
  <div class="row" style="align-items:center">
    <div style="min-width:4em">
      <div class="label">Battery</div>
      <div class="big" id="bat">&mdash;</div>
    </div>
    <div style="flex:1;margin-left:1.2em">
      <div style="background:#333;border-radius:99px;height:10px;overflow:hidden">
        <div id="bat-bar" style="height:10px;border-radius:99px;width:0%;transition:width 0.6s"></div>
      </div>
    </div>
  </div>
</div>
<div class="card">
  <div class="label">How the eyes pick a mood</div>
  <table class="legend">
    <tr><th>Temperature</th><th>Mood</th><th>Why</th></tr>
    <tr id="m-cold"><td>&lt; 20&deg;C</td><td>Surprised</td><td>cold</td></tr>
    <tr id="m-comfy"><td>20 &ndash; 28&deg;C</td><td>Happy</td><td>comfy</td></tr>
    <tr id="m-hot"><td>&gt; 28&deg;C</td><td>Sleepy</td><td>hot</td></tr>
  </table>
</div>
<script>
const $ = id => document.getElementById(id);
function fmtUptime(s) {
  if (s < 60) return s + "s";
  if (s < 3600) return Math.floor(s/60) + "m";
  return Math.floor(s/3600) + "h " + Math.floor((s%3600)/60) + "m";
}
function fmtNum(v, dec, suffix) {
  return v == null ? "—" : v.toFixed(dec) + (suffix || "");
}
// Render a sparkline into the given <polyline>/<path> pair.
// `history` is an array (oldest first) of numbers or nulls.
function renderChart(lineEl, areaEl, history) {
  const N = 60;
  const padded = new Array(N - history.length).fill(null).concat(history);
  const valid = padded.filter(v => v != null);
  if (valid.length < 2) { lineEl.setAttribute('points',''); areaEl.setAttribute('d',''); return; }
  const lo = Math.min(...valid) - 0.3;
  const hi = Math.max(...valid) + 0.3;
  const range = hi - lo || 1;
  const pts = [];
  padded.forEach((v, i) => {
    if (v != null) pts.push(i + ',' + (100 - (v - lo) / range * 100));
  });
  lineEl.setAttribute('points', pts.join(' '));
  const first = pts[0].split(',')[0], last = pts[pts.length-1].split(',')[0];
  areaEl.setAttribute('d', 'M' + first + ',100 L' + pts.join(' L') + ' L' + last + ',100 Z');
}
function highlightMood(temp) {
  ['m-cold','m-comfy','m-hot'].forEach(id => $(id).classList.remove('active'));
  if (temp == null) return;
  if (temp < 20)      $('m-cold').classList.add('active');
  else if (temp > 28) $('m-hot').classList.add('active');
  else                $('m-comfy').classList.add('active');
}
async function refresh() {
  let d;
  try { d = await (await fetch('/data')).json(); }
  catch (e) { document.title = "MiniC3 (offline)"; return; }
  document.title = "MiniC3";
  $('temp').textContent = fmtNum(d.temp, 1, "°");
  $('hum').textContent  = fmtNum(d.humidity, 0, "%");
  $('tMin').textContent = fmtNum(d.today.temp_min, 1, "°");
  $('tMax').textContent = fmtNum(d.today.temp_max, 1, "°");
  $('hMin').textContent = fmtNum(d.today.humidity_min, 0, "%");
  $('hMax').textContent = fmtNum(d.today.humidity_max, 0, "%");
  $('time').textContent = d.time || "—";
  $('mood').textContent = d.mood || "—";
  $('uptime').textContent = fmtUptime(d.uptime_s);
  renderChart($('lineT'), $('areaT'), d.history_t);
  renderChart($('lineH'), $('areaH'), d.history_h);
  highlightMood(d.temp);
  $('bat').textContent = d.battery != null ? d.battery + '%' : '—';
  const bar = $('bat-bar');
  bar.style.width = (d.battery || 0) + '%';
  bar.style.background = d.battery != null && d.battery <= 20 ? '#ff7c7c' : '#4ade80';
}
refresh();
setInterval(refresh, 5000);
</script>
</body>
</html>)HTML";

WebDashboard::WebDashboard(TemperatureSensor& sensor,
                           TemperatureHistory& history,
                           DailyStats& daily,
                           Eyes& eyes)
    : sensor_(sensor), history_(history), daily_(daily), eyes_(eyes),
      server_(80), batteryPercent_(-1) {}

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

void WebDashboard::setBattery(int percent) {
    batteryPercent_ = percent;
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
    json += ",\"battery\":";
    if (batteryPercent_ < 0) json += "null";
    else json += String(batteryPercent_);

    // Today's high/low for both metrics. NaN -> JSON null so the JS
    // can show "—" until at least one sample lands today.
    json += ",\"today\":{\"temp_min\":";
    appendNumberOrNull(daily_.tempMin(), 1);
    json += ",\"temp_max\":";
    appendNumberOrNull(daily_.tempMax(), 1);
    json += ",\"humidity_min\":";
    appendNumberOrNull(daily_.humidityMin(), 0);
    json += ",\"humidity_max\":";
    appendNumberOrNull(daily_.humidityMax(), 0);
    json += "}";

    // Two history arrays - temperature and humidity. Both are oldest-
    // first, only count() entries; the browser pads the left side
    // with nulls so the newest sample sits at x=59.
    json += ",\"history_t\":[";
    for (size_t i = 0; i < history_.count(); i++) {
        if (i > 0) json += ',';
        appendNumberOrNull(history_.tempAt(i), 1);
    }
    json += "],\"history_h\":[";
    for (size_t i = 0; i < history_.count(); i++) {
        if (i > 0) json += ',';
        appendNumberOrNull(history_.humidityAt(i), 0);
    }
    json += "]}";

    // CORS isn't needed (same-origin) but no-cache stops phones from
    // showing stale numbers if the user reloads quickly.
    server_.sendHeader("Cache-Control", "no-cache");
    server_.send(200, "application/json", json);
}
