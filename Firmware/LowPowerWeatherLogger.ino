#include <Arduino.h>

#include <Wire.h>

#include <WiFi.h>

#include <WebServer.h>

#include <Adafruit_INA219.h>

#include <ArduinoJson.h>

#include "LittleFS.h"

#include "esp_sleep.h"

#define INA219_SDA 21

#define INA219_SCL 22


#define HUMIDITY_PIN 34

#define TEMPERATURE_PIN 35


#define LED_PIN 25

#define BUTTON_PIN 27


#define SLEEP_TIME_SECONDS 180UL          // 3 minutes deep sleep

#define uS_TO_S_FACTOR 1000000ULL


#define WEB_MODE_TIMEOUT_MS 90000UL       // 1 min 30 s web mode max

#define VREF 3.3f

#define ADC_MAX 4095.0f


#define BATTERY_FULL 4.20f

#define BATTERY_EMPTY 3.00f


const char* AP_SSID =
"LowPowerSensor";


const char* AP_PASSWORD =
"12345678";


#define BATTERY_LOG_FILE "/battery_log.csv"


WebServer server(80);


Adafruit_INA219 ina219;


struct SensorData
{

  float temperature;

  float humidity;


  float batteryVoltage;

  float current_mA;

  float power_mW;


  float shuntVoltage_mV;

  float busVoltage;


  float batteryPercent;


  unsigned long readingNumber;

  unsigned long runtimeSeconds;


  bool valid;

};


RTC_DATA_ATTR SensorData data =
{

0,
0,

0,
0,
0,

0,
0,

0,

0,
0,

false

};



RTC_DATA_ATTR float startBatteryVoltage = 0;


bool webMode = false;

unsigned long webModeStartTime = 0;


float calculateBatteryPercent(float voltage)
{

  float percent =
  (
    (voltage - BATTERY_EMPTY)
    /
    (BATTERY_FULL - BATTERY_EMPTY)
  )
  * 100.0f;


  if(percent > 100)
    percent = 100;


  if(percent < 0)
    percent = 0;


  return percent;

}


String batteryStatus()
{

  if(data.batteryVoltage >= 4.0)
  {
    return "Healthy";
  }


  if(data.batteryVoltage >= 3.5)
  {
    return "Running Normally";
  }


  return "Low Battery";

}


float readAnalogVoltage(int pin)
{

  const int samples = 20;


  uint32_t total = 0;


  for(int i = 0; i < samples; i++)
  {

    total += analogRead(pin);

    delay(2);

  }


  float average =
      (float)total / samples;


  return
      (average / ADC_MAX) * VREF;

}


void readSHT30Analog()
{


  float temperatureVoltage =
      readAnalogVoltage(TEMPERATURE_PIN);



  float humidityVoltage =
      readAnalogVoltage(HUMIDITY_PIN);



  // DFR0588 analog conversion

  data.temperature =
      -66.875f +
      (72.917f * temperatureVoltage);



  data.humidity =
      -12.5f +
      (41.667f * humidityVoltage);



  if(data.humidity < 0)
    data.humidity = 0;


  if(data.humidity > 100)
    data.humidity = 100;


}


void readINA219()
{


  data.busVoltage =
      ina219.getBusVoltage_V();



  data.shuntVoltage_mV =
      ina219.getShuntVoltage_mV();



  data.current_mA =
      ina219.getCurrent_mA();



  data.power_mW =
      ina219.getPower_mW();



  data.batteryVoltage =
      data.busVoltage +
      (
        data.shuntVoltage_mV / 1000.0f
      );



  data.batteryPercent =
      calculateBatteryPercent(
        data.batteryVoltage
      );


}


void takeMeasurement()
{


  Serial.println();

  Serial.println("====================");

  Serial.println("Sensor Measurement");

  Serial.println("====================");



  // Sensor stabilization (10 seconds for DFR0588)

  delay(10000);



  readSHT30Analog();



  delay(100);



  readINA219();



  data.readingNumber++;



  data.runtimeSeconds =
      data.readingNumber *
      SLEEP_TIME_SECONDS;



  data.valid = true;



  // Save starting voltage

  if(startBatteryVoltage == 0)
  {

    startBatteryVoltage =
        data.batteryVoltage;

  }



  digitalWrite(
    LED_PIN,
    HIGH
  );

  delay(100);

  digitalWrite(
    LED_PIN,
    LOW
  );



  Serial.println();


  Serial.print(
    "Temperature: "
  );

  Serial.println(
    data.temperature
  );



  Serial.print(
    "Humidity: "
  );

  Serial.println(
    data.humidity
  );



  Serial.print(
    "Battery: "
  );

  Serial.println(
    data.batteryVoltage
  );

}


void startLittleFS()
{


  Serial.println(
    "Starting LittleFS..."
  );



  if(!LittleFS.begin(true))
  {

    Serial.println(
      "LittleFS failed"
    );


    return;

  }



  Serial.println(
    "LittleFS ready"
  );



  if(!LittleFS.exists(BATTERY_LOG_FILE))
  {


    File file =
        LittleFS.open(
          BATTERY_LOG_FILE,
          "w"
        );



    if(file)
    {


      file.println(
      "reading,time,voltage,current_mA,power_mW,temperature,humidity"
      );


      file.close();



      Serial.println(
        "CSV created"
      );

    }


  }

}


void saveBatteryLog()
{


  File file =
      LittleFS.open(
        BATTERY_LOG_FILE,
        "a"
      );



  if(!file)
  {

    Serial.println(
      "Cannot open CSV"
    );


    return;

  }



  file.print(
    data.readingNumber
  );


  file.print(",");



  file.print(
    data.runtimeSeconds
  );


  file.print(",");



  file.print(
    data.batteryVoltage,
    3
  );


  file.print(",");



  file.print(
    data.current_mA,
    2
  );


  file.print(",");



  file.print(
    data.power_mW,
    2
  );


  file.print(",");



  file.print(
    data.temperature,
    2
  );


  file.print(",");



  file.println(
    data.humidity,
    2
  );



  file.flush();

  file.close();



  Serial.println(
    "Battery data saved"
  );

}


void printLogSize()
{


  File file =
      LittleFS.open(
        BATTERY_LOG_FILE,
        "r"
      );



  if(file)
  {


    Serial.print(
      "Log size: "
    );


    Serial.print(
      file.size()
    );


    Serial.println(
      " bytes"
    );


    file.close();

  }


}


void handleDownload()
{


  if(!LittleFS.exists(BATTERY_LOG_FILE))
  {

    server.send(
      404,
      "text/plain",
      "No CSV file"
    );


    return;

  }



  File file =
      LittleFS.open(
        BATTERY_LOG_FILE,
        "r"
      );



  server.streamFile(
    file,
    "text/csv"
  );

  file.close();


}


void handleReset()
{


  // Remove old file if present

  if(LittleFS.exists(BATTERY_LOG_FILE))
  {

    LittleFS.remove(BATTERY_LOG_FILE);

  }



  // Re-create with just the header

  File file =
      LittleFS.open(
        BATTERY_LOG_FILE,
        "w"
      );



  if(file)
  {

    file.println(
      "reading,time,voltage,current_mA,power_mW,temperature,humidity"
    );

    file.close();

  }



 

  data.readingNumber = 0;

  data.runtimeSeconds = 0;

  startBatteryVoltage = 0;



  Serial.println(
    "Log reset"
  );



  server.send(
    200,
    "application/json",
    "{\"ok\":true}"
  );

}


void handleStatusAPI()
{

  StaticJsonDocument<512> doc;


  doc["temperature"] =
      data.temperature;


  doc["humidity"] =
      data.humidity;


  doc["batteryVoltage"] =
      data.batteryVoltage;


  doc["batteryPercent"] =
      data.batteryPercent;


  doc["current"] =
      data.current_mA;


  doc["power"] =
      data.power_mW;


  doc["runtimeSeconds"] =
      data.runtimeSeconds;


  doc["readings"] =
      data.readingNumber;


  doc["batteryStatus"] =
      batteryStatus();



  String output;


  serializeJson(
    doc,
    output
  );



  server.send(
    200,
    "application/json",
    output
  );

}


void handleHistoryAPI()
{


  if(!LittleFS.exists(BATTERY_LOG_FILE))
  {

    server.send(
      404,
      "application/json",
      "{\"error\":\"No data\"}"
    );


    return;

  }



  File file =
      LittleFS.open(
        BATTERY_LOG_FILE,
        "r"
      );



  String json =
      "{\"time\":[";



  bool first = true;



  

  file.readStringUntil('\n');



  while(file.available())
  {


    String line =
        file.readStringUntil('\n');



    int c1 =
        line.indexOf(',');



    if(c1 < 0)
      continue;



    String readingTime =
        line.substring(
          c1 + 1,
          line.indexOf(
            ',',
            c1 + 1
          )
        );



    if(!first)
      json += ",";


    json += readingTime;

    first = false;

  }



  file.close();



  json += "],\"voltage\":[";



  file =
      LittleFS.open(
        BATTERY_LOG_FILE,
        "r"
      );



  file.readStringUntil('\n');



  first = true;



  while(file.available())
  {

    String line =
        file.readStringUntil('\n');



    int c1 =
        line.indexOf(',');



    int c2 =
        line.indexOf(
          ',',
          c1 + 1
        );



    if(c2 < 0)
      continue;



    String voltage =
        line.substring(
          c2 + 1,
          line.indexOf(
            ',',
            c2 + 1
          )
        );



    if(!first)
      json += ",";


    json += voltage;

    first = false;

  }



  file.close();



  json += "]}";



  server.send(
    200,
    "application/json",
    json
  );

}


String buildWebPage()
{

String page = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0a0e13">
<title>Low Power Weather Station</title>
<style>

*,*::before,*::after{box-sizing:border-box}

:root{
  --bg:#0a0e13;
  --card:#131a23;
  --card2:#182231;
  --line:#22303f;
  --txt:#e9f1f9;
  --muted:#8598ad;
  --accent:#4cc2ff;
  --green:#3ddc97;
  --red:#ff5c7a;
}

html,body{margin:0;padding:0}

body{
  font-family:system-ui,-apple-system,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;
  background:radial-gradient(900px 520px at 50% -280px,#172433 0%,transparent 70%),var(--bg);
  color:var(--txt);
  min-height:100vh;
  padding:16px 14px 44px;
  -webkit-font-smoothing:antialiased;
}

.wrap{max-width:880px;margin:0 auto;display:flex;flex-direction:column;gap:14px}

/* ---------- header ---------- */

.head{display:flex;align-items:center;justify-content:space-between;gap:12px;flex-wrap:wrap;padding:2px 4px}
.head h1{margin:0;font-size:19px;font-weight:650;letter-spacing:.2px}
.head .sub{margin-top:4px;font-size:12px;color:var(--muted)}

.live{
  display:inline-flex;align-items:center;gap:8px;font-size:12px;color:var(--muted);
  background:var(--card);border:1px solid var(--line);
  padding:7px 12px;border-radius:999px;white-space:nowrap;
}
.dot{width:9px;height:9px;border-radius:50%;background:var(--muted);flex:0 0 auto}
.live.ok .dot{background:var(--green);animation:pulse 2s infinite}
.live.err .dot{background:var(--red)}

@keyframes pulse{
  0%{box-shadow:0 0 0 0 rgba(61,220,151,.55)}
  70%{box-shadow:0 0 0 9px rgba(61,220,151,0)}
  100%{box-shadow:0 0 0 0 rgba(61,220,151,0)}
}

/* ---------- cards ---------- */

.card{
  background:linear-gradient(180deg,var(--card2),var(--card));
  border:1px solid var(--line);
  border-radius:16px;
  padding:16px;
}
.card h2{margin:0 0 12px;font-size:14.5px;font-weight:600;letter-spacing:.3px}

.label{font-size:11.5px;color:var(--muted);text-transform:uppercase;letter-spacing:.9px}
.muted{color:var(--muted)}

/* ---------- battery ---------- */

.bat-head{display:flex;align-items:flex-start;justify-content:space-between;gap:14px;flex-wrap:wrap}

.big{
  font-size:clamp(28px,7vw,40px);
  font-weight:750;letter-spacing:-.5px;
  margin-top:4px;line-height:1.1;
}

.pill{
  display:inline-block;margin-top:6px;padding:6px 12px;border-radius:999px;
  font-size:12.5px;font-weight:600;
  border:1px solid var(--line);background:rgba(255,255,255,.04);color:var(--muted);
}
.pill.ok{color:var(--green);border-color:rgba(61,220,151,.35);background:rgba(61,220,151,.10)}
.pill.mid{color:var(--accent);border-color:rgba(76,194,255,.35);background:rgba(76,194,255,.10)}
.pill.low{color:var(--red);border-color:rgba(255,92,122,.35);background:rgba(255,92,122,.10)}

.bar{
  height:12px;border-radius:999px;background:#0c1219;
  border:1px solid var(--line);overflow:hidden;margin-top:16px;
}

.fill{
  height:100%;width:0%;border-radius:999px;
  background:linear-gradient(90deg,#3ddc97,#4cc2ff);
  transition:width .6s ease,background .3s ease;
}
.fill.low{background:linear-gradient(90deg,#ff5c7a,#ffc857)}

.bar-row{display:flex;justify-content:space-between;font-size:12.5px;margin-top:8px}

/* ---------- metrics ---------- */

.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:12px}
.metric{padding:14px}
.val{font-size:21px;font-weight:700;margin-top:8px;letter-spacing:-.3px;word-break:break-word}

/* ---------- chart ---------- */

.card-head{display:flex;align-items:center;justify-content:space-between;gap:10px;flex-wrap:wrap;margin-bottom:10px}
.card-head h2{margin:0}
.chart-meta{font-size:11.5px;color:var(--muted)}

.chart-box{position:relative;width:100%;height:250px}
.chart-box canvas{position:absolute;inset:0;width:100%;height:100%;display:block;touch-action:none}

.empty{
  position:absolute;inset:0;display:flex;align-items:center;justify-content:center;
  color:var(--muted);font-size:13px;pointer-events:none;
}

/* ---------- actions ---------- */

.actions{display:flex;gap:10px;flex-wrap:wrap;margin-top:14px}

.btn{
  display:inline-flex;align-items:center;gap:8px;
  padding:11px 16px;border-radius:11px;
  border:1px solid var(--line);
  background:var(--card2);color:var(--txt);
  font-size:13.5px;font-weight:600;text-decoration:none;cursor:pointer;
  transition:transform .12s ease,background .2s ease,border-color .2s ease;
  font-family:inherit;
}
.btn:hover{background:#1e2938;border-color:#31424f}
.btn:active{transform:scale(.97)}
.btn.primary{
  background:linear-gradient(135deg,#4cc2ff,#7c5cff);
  border-color:transparent;color:#06121b;
}
.btn.danger{
  color:#ffd4dd;
  border-color:rgba(255,92,122,.35);
  background:rgba(255,92,122,.10);
}
.btn.danger:hover{
  background:rgba(255,92,122,.20);
  border-color:rgba(255,92,122,.55);
}

.foot{text-align:center;font-size:11.5px;color:var(--muted);padding:4px}

@media (max-width:420px){
  body{padding:12px 10px 36px}
  .card{padding:14px}
  .chart-box{height:220px}
}

</style>
</head>
<body>

<div class="wrap">

  <div class="head">
    <div>
      <h1>🌦️ Low Power Weather Station</h1>
      <div class="sub">ESP32-WROOM · 18650 logger · deep-sleep node</div>
    </div>
    <div class="live" id="live"><span class="dot"></span><span id="liveTxt">connecting…</span></div>
  </div>

  <div class="card">
    <div class="bat-head">
      <div>
        <div class="label">Battery Voltage</div>
        <div class="big" id="battery">-- V</div>
      </div>
      <div style="text-align:right">
        <div class="label">Status</div>
        <div class="pill" id="status">--</div>
      </div>
    </div>
    <div class="bar"><div class="fill" id="barFill"></div></div>
    <div class="bar-row">
      <span id="percent">-- %</span>
      <span class="muted">3.00 V — 4.20 V</span>
    </div>
  </div>

  <div class="grid">
    <div class="card metric"><div class="label">🌡 Temperature</div><div class="val" id="temp">--</div></div>
    <div class="card metric"><div class="label">💧 Humidity</div><div class="val" id="humidity">--</div></div>
    <div class="card metric"><div class="label">⚡ Power</div><div class="val" id="power">--</div></div>
    <div class="card metric"><div class="label">🔌 Current</div><div class="val" id="current">--</div></div>
    <div class="card metric"><div class="label">⏱ Runtime</div><div class="val" id="runtime">--</div></div>
    <div class="card metric"><div class="label">🧮 Readings</div><div class="val" id="readings">--</div></div>
  </div>

  <div class="card">
    <div class="card-head">
      <h2>📈 Battery History</h2>
      <div class="chart-meta" id="chartMeta">loading…</div>
    </div>
    <div class="chart-box">
      <canvas id="graph"></canvas>
      <div class="empty" id="chartEmpty">No history yet</div>
    </div>
  </div>

  <div class="card">
    <h2>📁 Battery Test Data</h2>
    <div class="muted" style="font-size:13px;line-height:1.5">
      Every wake-up is appended to <b>battery_log.csv</b> on LittleFS —
      voltage, current, power, temperature and humidity.
    </div>
    <div class="actions">
      <a class="btn primary" href="/download">⬇ Download CSV</a>
      <button class="btn" id="refreshBtn">↻ Refresh now</button>
      <button class="btn danger" id="resetBtn">🗑 Reset Log</button>
    </div>
  </div>

  <div class="foot" id="foot">waiting for data…</div>

</div>

<script>
(function(){
"use strict";

var $ = function(id){ return document.getElementById(id); };

function isNum(v){ return typeof v === "number" && isFinite(v); }
function fix(v,d){ return isNum(v) ? v.toFixed(d) : "--"; }

function fmtRuntime(sec){
  if(!isNum(sec)) return "--";
  sec = Math.max(0, Math.floor(sec));
  var d = Math.floor(sec/86400);
  var h = Math.floor((sec%86400)/3600);
  var m = Math.floor((sec%3600)/60);
  if(d > 0) return d + "d " + h + "h";
  if(h > 0) return h + "h " + m + "m";
  return m + "m";
}

function fmtAxisTime(sec){
  if(!isNum(sec)) return "";
  sec = Math.floor(sec);
  if(sec >= 86400) return Math.floor(sec/86400) + "d " + Math.floor((sec%86400)/3600) + "h";
  if(sec >= 3600)  return Math.floor(sec/3600) + "h " + Math.floor((sec%3600)/60) + "m";
  return Math.floor(sec/60) + "m";
}

/* ---------------- live status ---------------- */

var failCount = 0;

function setLive(cls, text){
  $("live").className = "live " + cls;
  $("liveTxt").textContent = text;
}

function applyStatus(d){
  $("battery").textContent  = fix(d.batteryVoltage,3) + " V";
  $("temp").textContent     = fix(d.temperature,1) + " °C";
  $("humidity").textContent = fix(d.humidity,1) + " %";
  $("power").textContent    = fix(d.power,1) + " mW";
  $("current").textContent  = fix(d.current,1) + " mA";
  $("runtime").textContent  = fmtRuntime(d.runtimeSeconds);
  $("readings").textContent = isNum(d.readings) ? d.readings : "--";

  var pct = isNum(d.batteryPercent) ? Math.max(0, Math.min(100, d.batteryPercent)) : 0;
  var fill = $("barFill");
  fill.style.width = pct + "%";
  fill.className = "fill" + (pct < 20 ? " low" : "");
  $("percent").textContent = isNum(d.batteryPercent) ? pct.toFixed(0) + " %" : "-- %";

  var s = String(d.batteryStatus || "--");
  var pill = $("status");
  pill.textContent = s;
  pill.className = "pill" + (s.indexOf("Low") >= 0 ? " low" : (s.indexOf("Healthy") >= 0 ? " ok" : " mid"));

  $("foot").textContent =
    "last reading: " + new Date().toLocaleTimeString() +
    "  ·  " + (isNum(d.readings) ? d.readings : 0) + " samples logged";
}

function updateStatus(){
  if(document.hidden) return;

  fetch("/api/status", {cache:"no-store"})
    .then(function(r){
      if(!r.ok) throw new Error("http " + r.status);
      return r.json();
    })
    .then(function(d){
      failCount = 0;
      applyStatus(d);
      setLive("ok", "live · 1s");
    })
    .catch(function(){
      failCount++;
      if(failCount >= 2) setLive("err", "connection lost");
    });
}

/* ---------------- history chart ---------------- */

var cv = $("graph");
var cx = cv.getContext("2d");
var hist = { time: [], voltage: [] };
var hover = -1;
var PAD = { l: 48, r: 14, t: 16, b: 30 };

function roundRect(ctx,x,y,w,h,r){
  ctx.beginPath();
  ctx.moveTo(x+r,y);
  ctx.arcTo(x+w,y,x+w,y+h,r);
  ctx.arcTo(x+w,y+h,x,y+h,r);
  ctx.arcTo(x,y+h,x,y,r);
  ctx.arcTo(x,y,x+w,y,r);
  ctx.closePath();
}

function sizeCanvas(){
  var box = cv.parentElement;
  var dpr = window.devicePixelRatio || 1;
  var w = Math.max(240, box.clientWidth);
  var h = Math.max(160, box.clientHeight);
  cv.width  = Math.round(w * dpr);
  cv.height = Math.round(h * dpr);
  cx.setTransform(dpr,0,0,dpr,0,0);
  drawChart();
}

function drawChart(){
  var w = cv.clientWidth  || 300;
  var h = cv.clientHeight || 240;
  cx.clearRect(0,0,w,h);

  var v = hist.voltage;

  if(!v || v.length === 0){
    $("chartEmpty").style.display = "flex";
    return;
  }
  $("chartEmpty").style.display = "none";

  var plotW = w - PAD.l - PAD.r;
  var plotH = h - PAD.t - PAD.b;

  var min = Math.min.apply(null, v);
  var max = Math.max.apply(null, v);

  if(max - min < 0.02){
    var mid = (max + min) / 2;
    min = mid - 0.01;
    max = mid + 0.01;
  } else {
    var pad = (max - min) * 0.15;
    min -= pad;
    max += pad;
  }

  function X(i){
    if(v.length === 1) return PAD.l + plotW / 2;
    return PAD.l + (i / (v.length - 1)) * plotW;
  }

  function Y(val){
    return PAD.t + plotH - ((val - min) / (max - min)) * plotH;
  }

  /* grid + voltage labels */
  cx.font = "11px system-ui,-apple-system,sans-serif";
  cx.textAlign = "right";
  cx.textBaseline = "middle";
  cx.lineWidth = 1;

  for(var r = 0; r <= 4; r++){
    var val = min + (max - min) * (r / 4);
    var y = Y(val);

    cx.strokeStyle = "rgba(255,255,255,0.07)";
    cx.beginPath();
    cx.moveTo(PAD.l, y);
    cx.lineTo(PAD.l + plotW, y);
    cx.stroke();

    cx.fillStyle = "rgba(180,200,220,0.55)";
    cx.fillText(val.toFixed(2) + "V", PAD.l - 8, y);
  }

  /* time labels */
  cx.textBaseline = "top";
  cx.fillStyle = "rgba(180,200,220,0.55)";

  var marks = [0, Math.floor((v.length - 1) / 2), v.length - 1];
  var used = {};

  for(var m = 0; m < marks.length; m++){
    var idx = marks[m];
    if(used[idx]) continue;
    used[idx] = true;

    var tx = X(idx);
    var label = (hist.time && hist.time.length > idx)
      ? fmtAxisTime(hist.time[idx])
      : ("#" + (idx + 1));

    if(m === 0) cx.textAlign = "left";
    else if(m === marks.length - 1) cx.textAlign = "right";
    else cx.textAlign = "center";

    cx.fillText(label, tx, PAD.t + plotH + 8);
  }

  /* area fill */
  if(v.length > 1){
    var grad = cx.createLinearGradient(0, PAD.t, 0, PAD.t + plotH);
    grad.addColorStop(0, "rgba(76,194,255,0.32)");
    grad.addColorStop(1, "rgba(76,194,255,0.01)");

    cx.beginPath();
    cx.moveTo(X(0), Y(v[0]));
    for(var a = 1; a < v.length; a++) cx.lineTo(X(a), Y(v[a]));
    cx.lineTo(X(v.length - 1), PAD.t + plotH);
    cx.lineTo(X(0), PAD.t + plotH);
    cx.closePath();
    cx.fillStyle = grad;
    cx.fill();
  }

  /* line */
  if(v.length > 1){
    cx.beginPath();
    cx.moveTo(X(0), Y(v[0]));
    for(var b = 1; b < v.length; b++) cx.lineTo(X(b), Y(v[b]));

    cx.lineWidth = 2.5;
    cx.lineJoin = "round";
    cx.lineCap = "round";
    cx.strokeStyle = "#4cc2ff";
    cx.shadowColor = "rgba(76,194,255,0.6)";
    cx.shadowBlur = 12;
    cx.stroke();
    cx.shadowBlur = 0;
  }

  /* last point */
  var li = v.length - 1;
  cx.beginPath();
  cx.arc(X(li), Y(v[li]), 4.5, 0, Math.PI * 2);
  cx.fillStyle = "#4cc2ff";
  cx.fill();
  cx.lineWidth = 2;
  cx.strokeStyle = "rgba(10,14,19,0.9)";
  cx.stroke();

  /* hover marker + tooltip */
  if(hover >= 0 && hover < v.length){
    var hx = X(hover);
    var hy = Y(v[hover]);

    cx.setLineDash([4,4]);
    cx.strokeStyle = "rgba(255,255,255,0.22)";
    cx.lineWidth = 1;
    cx.beginPath();
    cx.moveTo(hx, PAD.t);
    cx.lineTo(hx, PAD.t + plotH);
    cx.stroke();
    cx.setLineDash([]);

    cx.beginPath();
    cx.arc(hx, hy, 5, 0, Math.PI * 2);
    cx.fillStyle = "#ffffff";
    cx.fill();

    var txt = v[hover].toFixed(3) + " V";
    var sub = (hist.time && hist.time.length > hover) ? fmtAxisTime(hist.time[hover]) : "";

    cx.font = "600 12px system-ui,-apple-system,sans-serif";
    var tw = cx.measureText(txt).width;
    var bw = tw + 20;
    var bh = sub ? 42 : 28;

    var bx = hx + 14;
    if(bx + bw > w - 6) bx = hx - 14 - bw;

    var by = Math.min(Math.max(hy - bh / 2, 6), h - bh - 6);

    roundRect(cx, bx, by, bw, bh, 9);
    cx.fillStyle = "rgba(9,14,20,0.94)";
    cx.fill();
    cx.strokeStyle = "rgba(76,194,255,0.45)";
    cx.lineWidth = 1;
    cx.stroke();

    cx.textAlign = "left";
    cx.textBaseline = "top";
    cx.fillStyle = "#e9f1f9";
    cx.fillText(txt, bx + 10, by + 8);

    if(sub){
      cx.font = "11px system-ui,-apple-system,sans-serif";
      cx.fillStyle = "#8598ad";
      cx.fillText(sub, bx + 10, by + 25);
    }
  }
}

function loadHistory(){
  if(document.hidden) return;

  fetch("/api/history", {cache:"no-store"})
    .then(function(r){
      if(!r.ok) throw new Error("no data");
      return r.json();
    })
    .then(function(d){
      var times = Array.isArray(d.time) ? d.time : [];
      var volts = Array.isArray(d.voltage)
        ? d.voltage.map(Number).filter(isFinite)
        : [];

      hist.time = times;
      hist.voltage = volts;

      if(hover >= volts.length) hover = -1;
      drawChart();

      if(volts.length === 0){
        $("chartMeta").textContent = "no samples";
      } else {
        $("chartMeta").textContent =
          volts.length + " samples · " +
          Math.min.apply(null, volts).toFixed(3) + "–" +
          Math.max.apply(null, volts).toFixed(3) + " V";
      }
    })
    .catch(function(){
      hist.time = [];
      hist.voltage = [];
      drawChart();
      $("chartMeta").textContent = "no samples";
    });
}

/* ---------------- interaction ---------------- */

function pickPoint(clientX){
  var v = hist.voltage;
  if(v.length < 1) return;

  var rect = cv.getBoundingClientRect();
  var plotW = rect.width - PAD.l - PAD.r;
  var t = (clientX - rect.left - PAD.l) / plotW;

  var i = Math.round(t * Math.max(1, v.length - 1));
  i = Math.max(0, Math.min(v.length - 1, i));

  if(i !== hover){
    hover = i;
    drawChart();
  }
}

cv.addEventListener("pointerdown", function(e){ pickPoint(e.clientX); });

cv.addEventListener("pointermove", function(e){
  if(e.pointerType === "touch" && e.buttons === 0) return;
  pickPoint(e.clientX);
});

cv.addEventListener("pointerleave", function(){
  if(hover !== -1){ hover = -1; drawChart(); }
});

cv.addEventListener("pointerup", function(){
  if(hover !== -1){ hover = -1; drawChart(); }
});

$("refreshBtn").addEventListener("click", function(){
  var btn = this;
  btn.textContent = "↻ Refreshing…";
  updateStatus();
  loadHistory();
  setTimeout(function(){ btn.textContent = "↻ Refresh now"; }, 700);
});

$("resetBtn").addEventListener("click", function(){
  if(!confirm("Delete all logged data? This cannot be undone.")) return;

  var btn = this;
  btn.textContent = "🗑 Resetting…";
  btn.disabled = true;

  fetch("/reset", {method:"POST", cache:"no-store"})
    .then(function(r){ return r.json(); })
    .then(function(){
      hist.time = [];
      hist.voltage = [];
      hover = -1;
      drawChart();
      $("chartMeta").textContent = "no samples";
      updateStatus();
      loadHistory();
      btn.textContent = "✓ Log reset";
      setTimeout(function(){
        btn.textContent = "🗑 Reset Log";
        btn.disabled = false;
      }, 1200);
    })
    .catch(function(){
      btn.textContent = "⚠ Reset failed";
      setTimeout(function(){
        btn.textContent = "🗑 Reset Log";
        btn.disabled = false;
      }, 1500);
    });
});

window.addEventListener("resize", sizeCanvas);

document.addEventListener("visibilitychange", function(){
  if(!document.hidden){
    updateStatus();
    loadHistory();
  }
});

/* ---------------- boot ---------------- */

sizeCanvas();
updateStatus();
loadHistory();

setInterval(updateStatus, 1000);   /* live values every second */
setInterval(loadHistory, 5000);    /* chart every 5 seconds     */

})();
</script>

</body>
</html>
)rawliteral";

return page;

}


void handleRoot()
{

server.send(
200,
"text/html",
buildWebPage()
);

}


void startWebMode()
{


webMode = true;

webModeStartTime = millis();



Serial.println();

Serial.println(
"Starting Web Mode"
);



WiFi.mode(
WIFI_AP
);



WiFi.softAP(
AP_SSID,
AP_PASSWORD
);



Serial.print(
"WiFi Name: "
);

Serial.println(
AP_SSID
);



Serial.print(
"IP Address: "
);

Serial.println(
WiFi.softAPIP()
);



Serial.println(
"Auto-sleep after 90 seconds"
);




server.on(
"/",
handleRoot
);



server.on(
"/api/status",
handleStatusAPI
);



server.on(
"/api/history",
handleHistoryAPI
);



server.on(
"/download",
handleDownload
);



server.on(
"/reset",
handleReset
);



server.begin();



Serial.println(
"Web server started"
);

}


void stopWebMode()
{


Serial.println(
"Stopping Web Mode"
);



server.stop();



WiFi.softAPdisconnect(
true
);



WiFi.mode(
WIFI_OFF
);



webMode = false;



}


bool buttonPressed()
{

return digitalRead(
BUTTON_PIN
)
==
LOW;

}


void enterDeepSleep()
{


Serial.println();

Serial.println(
"Entering Deep Sleep"
);



WiFi.disconnect(
true
);



WiFi.mode(
WIFI_OFF
);


esp_sleep_enable_ext0_wakeup(
GPIO_NUM_27,
0
);


esp_sleep_enable_timer_wakeup(

SLEEP_TIME_SECONDS
*
uS_TO_S_FACTOR

);



Serial.print(
"Sleeping for "
);



Serial.print(
SLEEP_TIME_SECONDS
);



Serial.println(
" seconds"
);



Serial.flush();



delay(100);



esp_deep_sleep_start();



}


void setup()
{


Serial.begin(
115200
);



delay(500);



Serial.println();

Serial.println(
"======================"
);

Serial.println(
"LOW POWER WEATHER LOGGER"
);

Serial.println(
"ESP32-WROOM-DA"
);

Serial.println(
"======================"
);


pinMode(
LED_PIN,
OUTPUT
);


digitalWrite(
LED_PIN,
LOW
);



pinMode(
BUTTON_PIN,
INPUT_PULLUP
);


analogReadResolution(
12
);



analogSetPinAttenuation(
HUMIDITY_PIN,
ADC_11db
);



analogSetPinAttenuation(
TEMPERATURE_PIN,
ADC_11db
);


startLittleFS();


Wire.begin(
INA219_SDA,
INA219_SCL
);


if(!ina219.begin())
{

Serial.println(
"INA219 not detected"
);

}
else
{

Serial.println(
"INA219 ready"
);


ina219.setCalibration_32V_2A();

}


esp_sleep_wakeup_cause_t wakeReason;



wakeReason =
esp_sleep_get_wakeup_cause();



Serial.print(
"Wake reason: "
);



Serial.println(
(int)wakeReason
);


takeMeasurement();



saveBatteryLog();



printLogSize();



if(
wakeReason ==
ESP_SLEEP_WAKEUP_EXT0
)

{


Serial.println(
"Button wake"
);



while(buttonPressed())
{

delay(20);

}



startWebMode();



return;


}


enterDeepSleep();


}


void loop()
{


if(!webMode)
{

return;

}



server.handleClient();



if(
millis() - webModeStartTime
>=
WEB_MODE_TIMEOUT_MS
)

{


Serial.println(
"Web mode timeout (90s) - sleeping"
);



stopWebMode();



delay(500);



enterDeepSleep();

}



delay(10);



}