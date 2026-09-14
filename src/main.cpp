/*
 * Aquarium Light Controller
 * PlatformIO project
 * - Instant mode changes
 * - 8-slot schedule with ramping
 * - Web interface with schedule editing
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <time.h>
#include <sys/time.h>
#include "secrets.h"

// -------------------- Configuration --------------------
#define PWM_PIN        21
#define PWM_CHANNEL    0
#define PWM_FREQ       1000
#define PWM_RESOLUTION 8
#define WIFI_TIMEOUT   30
#define NUM_SLOTS      8
#define UPDATE_INTERVAL 50
#define RAMP_DURATION  5.0
#define OVERRIDE_RAMP_DURATION 5
#define RAMP_FALLBACK_DELAY 1000
#define NTP_SYNC_INTERVAL 3600
#define TIMEZONE "SAST-2"
#define AUTO_FALLBACK 0.0

// -------------------- Default Slots --------------------
const int defaultSlots[NUM_SLOTS][6] = {
  {0,0, 6,0, 2,100},
  {6,0, 8,0, 0,100},
  {8,0, 18,0, 2,100},
  {18,0, 20,0, 1,100},
  {20,0, 24,0, 2,0},
  {0,0,0,0,2,0},
  {0,0,0,0,2,0},
  {0,0,0,0,2,0}
};

const char* modeNames[] = {"Ramp Up", "Ramp Down", "Fixed"};
const uint8_t OVERRIDE_AUTO = 0;
const uint8_t OVERRIDE_ON = 1;
const uint8_t OVERRIDE_OFF = 2;
const uint8_t overrideLevels[] = {0, 100, 0, 15, 30, 45, 60, 75, 90};
const char* overrideLabels[] = {"Auto", "100%", "Off", "15%", "30%", "45%", "60%", "75%", "90%"};

// -------------------- HTML (PROGMEM) --------------------
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Aquarium Light</title>
<style>
body{font-family:Arial;margin:10px;background:#f4f4f4;max-width:800px;margin:auto}
.container{background:#fff;padding:15px;border-radius:8px}
h1{margin:0 0 10px 0;font-size:22px}
.ov{display:flex;align-items:center;gap:10px;padding:6px 10px;background:#ecf0f1;border-radius:4px;margin:8px 0}
.buttons{margin:10px 0;display:flex;gap:10px;flex-wrap:wrap}
.btn{background:#3498db;color:#fff;border:none;padding:8px 16px;border-radius:4px;cursor:pointer;font-size:14px;text-decoration:none}
.btn:hover{background:#2980b9}
.btn-intensity{background:#2ecc71}
.btn-intensity:hover,.btn-intensity.selected{background:#198f4d}
.btn.selected{box-shadow:0 0 0 3px #2c3e50 inset;font-weight:700}
.btn-off{background:#e74c3c}
.btn-off:hover{background:#c0392b}
.btn-auto{background:#f39c12}
.btn-auto:hover{background:#e67e22}
table{width:100%;border-collapse:collapse;font-size:13px}
th,td{padding:4px;text-align:center;border-bottom:1px solid #ddd}
th{background:#3498db;color:#fff}
input[type=time]{width:80px}
input[type=number]{width:50px}
select{font-size:12px}
.footer{margin-top:10px;font-size:11px;color:#7f8c8d;text-align:center}
</style>
</head>
<body>
<div class=container>
<h1>🐠 Aquarium Light</h1>
<div><b>Intensity:</b> <span id=i>0</span>%</div>
<div><b>Slot:</b> <span id=s>None</span></div>
<div><b>Time:</b> <span id=t>--</span></div>
<div class=ov>
<label>Mode:</label>
<span id=ms></span>
</div>
<div class=buttons>
<button class="btn btn-auto" onclick="setMode(0)">Auto</button>
<button class="btn btn-intensity" data-mode=3 onclick="setMode(3)">15%</button>
<button class="btn btn-intensity" data-mode=4 onclick="setMode(4)">30%</button>
<button class="btn btn-intensity" data-mode=5 onclick="setMode(5)">45%</button>
<button class="btn btn-intensity" data-mode=6 onclick="setMode(6)">60%</button>
<button class="btn btn-intensity" data-mode=7 onclick="setMode(7)">75%</button>
<button class="btn btn-intensity" data-mode=8 onclick="setMode(8)">90%</button>
<button class="btn btn-intensity" data-mode=1 onclick="setMode(1)">100%</button>
<button class="btn btn-off" onclick="setMode(2)">Off</button>
</div>
<h2>Schedule</h2>
<table><thead><tr><th>#</th><th>Start</th><th>End</th><th>Mode</th><th>Intensity</th></tr></thead>
<tbody id=body></tbody></table>
<div style=margin:8px 0>
<button class="btn" style="background:#2ecc71" onclick=saveSlots()>Save</button>
<button class="btn" style="background:#e74c3c" onclick=resetAll()>Reset</button>
<span id=msg style=color:#27ae60;margin-left:10px></span>
</div>
<div class=footer>00:00-00:00 disables a slot</div>
</div>
<script>
var modes=["Ramp Up","Ramp Down","Fixed"];
function dis(s,e){return s==="00:00"&&e==="00:00"}
async function loadSlots(){
 try{
  var r=await fetch("/api/slots");
  if(!r.ok) throw new Error("HTTP "+r.status);
  var d=await r.json();
  var tb=document.getElementById("body");
  tb.innerHTML="";
  for(var i=0;i<d.length;i++){
   var s=d[i],tr=document.createElement("tr");
   if(dis(s.startTime,s.endTime))tr.style.opacity=0.4;
   tr.innerHTML="<td>"+(i+1)+"</td><td><input type=time id=st"+i+" value='"+s.startTime+"'></td><td><input type=time id=en"+i+" value='"+s.endTime+"'></td><td><select id=mo"+i+">"+modes.map(function(m){return"<option value='"+m+"'"+(m===s.mode?" selected":"")+">"+m+"</option>"}).join("")+"</select></td><td><input type=number id=in"+i+" value='"+s.intensity+"' min=0 max=100></td>";
   tb.appendChild(tr);
  }
 }catch(e){ console.log("loadSlots error",e); }
}
async function saveSlots(){
 var slots=[];
 for(var i=0;i<8;i++){
  slots.push({startTime:document.getElementById("st"+i).value,endTime:document.getElementById("en"+i).value,mode:document.getElementById("mo"+i).value,intensity:parseInt(document.getElementById("in"+i).value)||0});
 }
 try{
  var r=await fetch("/api/slots",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(slots)});
  if(!r.ok) throw new Error("HTTP "+r.status);
  var msg=await r.text();
  document.getElementById("msg").textContent="✅ "+msg;
  setTimeout(function(){document.getElementById("msg").innerHTML="";},3000);
  loadSlots();
 }catch(e){ console.log(e); }
}
async function setMode(mode){
 try{
  var r=await fetch("/api/mode?mode="+mode,{method:"POST"});
  if(!r.ok) throw new Error("mode "+r.status);
  await loadMode();
  await updStatus();
 }catch(e){ console.log("setMode error",e); }
}
async function resetAll(){
 if(!confirm("Reset all?"))return;
 for(var i=0;i<8;i++){
  document.getElementById("st"+i).value="00:00";
  document.getElementById("en"+i).value="00:00";
  document.getElementById("mo"+i).value="Fixed";
  document.getElementById("in"+i).value=0;
 }
 await saveSlots();
}
async function loadMode(){
 try{
  var r=await fetch("/api/mode");
  if(!r.ok) throw new Error("mode "+r.status);
  var d=await r.json();
  var color=d.mode===0?"#3498db":(d.mode===2?"#e74c3c":"#198f4d");
  document.getElementById("ms").innerHTML="<span style='color:"+color+";font-weight:700'>"+d.label+"</span>";
  document.querySelectorAll(".buttons .btn").forEach(function(button){
   var selected=(button.dataset.mode!==undefined && Number(button.dataset.mode)===d.mode) ||
     (d.mode===0 && button.textContent.indexOf("Auto")===0) ||
     (d.mode===2 && button.textContent.indexOf("Off")===0);
   button.classList.toggle("selected",selected);
   if(selected && !button.textContent.endsWith(" ✓")) button.textContent+=" ✓";
   if(!selected) button.textContent=button.textContent.replace(" ✓","");
  });
 }catch(e){ console.log(e); }
}
async function updStatus(){
 try{
  var r=await fetch("/api/status?_="+Date.now());
  if(!r.ok) throw new Error("status "+r.status);
  var d=await r.json();
  document.getElementById("i").textContent=Math.round(d.intensity);
  document.getElementById("t").textContent=d.currentTime||"--";
  document.getElementById("s").textContent=d.activeSlot||"None";
 }catch(e){ console.log(e); }
}
loadSlots();loadMode();updStatus();
setInterval(updStatus,3000);
</script>
</body></html>
)rawliteral";

// -------------------- Globals --------------------
WebServer server(80);
Preferences preferences;

struct Slot {
  uint8_t startHour, startMinute, endHour, endMinute, mode, intensity;
  bool enabled;
};
Slot slots[NUM_SLOTS];
Slot newSlots[NUM_SLOTS];

uint8_t overrideMode = 0;
float currentPercent = 0.0;
float targetPercent = 0.0;
float lastScheduledIntensity = AUTO_FALLBACK;
float overrideRampStart = 0.0;
float overrideRampEnd = 0.0;
unsigned long overrideRampStartTime = 0;
float currentPWMValue = 0.0;
bool overrideRamping = false;
unsigned long lastRampProgressTime = 0;
unsigned long rampCounter = 0;

bool wifiConnected = false;
bool timeSynced = false;
unsigned long lastUpdateTime = 0;
unsigned long startupTime = 0;
unsigned long lastNtpSync = 0;
bool slotsNeedSave = false;

// -------------------- Helper Functions --------------------
int timeToSeconds(int h, int m, int s) { return h * 3600 + m * 60 + s; }
bool isSlotDisabled(int sh, int sm, int eh, int em) {
  return (sh == 0 && sm == 0 && eh == 0 && em == 0);
}
uint8_t overrideIntensity(uint8_t mode) {
  return mode < (sizeof(overrideLevels) / sizeof(overrideLevels[0])) ? overrideLevels[mode] : 0;
}
const char* overrideLabel(uint8_t mode) {
  return mode < (sizeof(overrideLabels) / sizeof(overrideLabels[0])) ? overrideLabels[mode] : "Auto";
}

void saveSlotsNow();

// -------------------- Preferences --------------------
void loadSlots() {
  preferences.begin("light", false);
  bool hasSaved = false;
  for (int i = 0; i < NUM_SLOTS; i++) {
    char key[10];
    sprintf(key, "slot%d", i);
    String data = preferences.getString(key, "");
    if (data.length() > 0) {
      hasSaved = true;
      int sh, sm, eh, em, mode, inten;
      sscanf(data.c_str(), "%d,%d,%d,%d,%d,%d", &sh, &sm, &eh, &em, &mode, &inten);
      slots[i].startHour = sh; slots[i].startMinute = sm;
      slots[i].endHour = eh; slots[i].endMinute = em;
      slots[i].mode = mode; slots[i].intensity = inten;
      slots[i].enabled = !isSlotDisabled(sh, sm, eh, em);
    } else {
      slots[i].startHour = defaultSlots[i][0];
      slots[i].startMinute = defaultSlots[i][1];
      slots[i].endHour = defaultSlots[i][2];
      slots[i].endMinute = defaultSlots[i][3];
      slots[i].mode = defaultSlots[i][4];
      slots[i].intensity = defaultSlots[i][5];
      slots[i].enabled = !isSlotDisabled(defaultSlots[i][0], defaultSlots[i][1],
                                         defaultSlots[i][2], defaultSlots[i][3]);
    }
  }
  preferences.end();
  if (!hasSaved) saveSlotsNow();
}

void saveSlotsNow() {
  preferences.begin("light", false);
  for (int i = 0; i < NUM_SLOTS; i++) {
    char key[10];
    sprintf(key, "slot%d", i);
    String data = String(slots[i].startHour) + "," + String(slots[i].startMinute) + "," +
                  String(slots[i].endHour) + "," + String(slots[i].endMinute) + "," +
                  String(slots[i].mode) + "," + String(slots[i].intensity);
    preferences.putString(key, data);
  }
  preferences.end();
  slotsNeedSave = false;
}

void loadOverrideMode() {
  preferences.begin("light", false);
  overrideMode = preferences.getUChar("mode", 0);
  preferences.end();
}

void saveOverrideModeNow() {
  preferences.begin("light", false);
  preferences.putUChar("mode", overrideMode);
  preferences.end();
}

// -------------------- PWM & Ramping --------------------
void setPWM(uint8_t value) {
  ledcWrite(PWM_CHANNEL, value);
}

void updatePWM() {
  float step = (100.0 / RAMP_DURATION) * (UPDATE_INTERVAL / 1000.0) / 100.0 * 255.0;
  float targetVal = (targetPercent / 100.0) * 255.0;
  if (overrideMode == 0) {
    currentPWMValue = targetVal;
    setPWM((uint8_t)round(currentPWMValue));
    currentPercent = targetPercent;
    return;
  }
  float diff = targetVal - currentPWMValue;
  if (abs(diff) < step) {
    currentPWMValue = targetVal;
  } else {
    currentPWMValue += (diff > 0) ? step : -step;
  }
  currentPWMValue = constrain(currentPWMValue, 0.0, 255.0);
  setPWM((uint8_t)round(currentPWMValue));
  currentPercent = (currentPWMValue / 255.0) * 100.0;
}

// -------------------- Schedule Calculation --------------------
float calculateScheduledIntensity(float currentSeconds) {
  int activeIndex = -1;
  float frac = 0.0;
  for (int i = 0; i < NUM_SLOTS; i++) {
    if (!slots[i].enabled) continue;
    int start = timeToSeconds(slots[i].startHour, slots[i].startMinute, 0);
    int end = timeToSeconds(slots[i].endHour, slots[i].endMinute, 0);
    bool contains = false;
    if (end > start) {
      contains = (currentSeconds >= start && currentSeconds < end);
      if (contains) frac = (float)(currentSeconds - start) / (float)(end - start);
    } else if (end < start) {
      contains = (currentSeconds >= start || currentSeconds < end);
      if (contains) {
        int duration = (24 * 3600 - start) + end;
        int elapsed = (currentSeconds >= start)
          ? (currentSeconds - start)
          : (currentSeconds + 24 * 3600 - start);
        frac = (float)elapsed / (float)duration;
      }
    }
    if (contains) {
      activeIndex = i;
      break;
    }
  }
  if (activeIndex == -1) return lastScheduledIntensity;
  frac = constrain(frac, 0.0, 1.0);

  int prevIndex = activeIndex - 1;
  if (prevIndex < 0) prevIndex = NUM_SLOTS - 1;
  while (!slots[prevIndex].enabled && prevIndex != activeIndex) {
    prevIndex--;
    if (prevIndex < 0) prevIndex = NUM_SLOTS - 1;
  }

  float startIntensity = (prevIndex != activeIndex && slots[prevIndex].enabled)
    ? slots[prevIndex].intensity
    : 0.0;
  float endIntensity = slots[activeIndex].intensity;
  float result = slots[activeIndex].mode == 2
    ? endIntensity
    : startIntensity + (endIntensity - startIntensity) * frac;
  lastScheduledIntensity = constrain(result, 0.0, 100.0);
  return lastScheduledIntensity;
}

float calculateTargetIntensity() {
  if (overrideMode != OVERRIDE_AUTO) return overrideIntensity(overrideMode);
  time_t now = time(nullptr);
  float currentSeconds;
  if (now > 10000 && timeSynced) {
    struct timeval timeValue;
    gettimeofday(&timeValue, nullptr);
    struct tm timeinfo;
    localtime_r(&timeValue.tv_sec, &timeinfo);
    currentSeconds = timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec +
                     (timeValue.tv_usec / 1000000.0f);
  } else {
    float elapsed = (millis() - startupTime) / 1000.0f;
    currentSeconds = fmodf(elapsed, 86400.0f);
  }
  return calculateScheduledIntensity(currentSeconds);
}

// -------------------- Override Ramp --------------------
void startOverrideRamp(uint8_t mode) {
  unsigned long startTime = millis();
  rampCounter++;

  if (overrideRamping) {
    Serial.printf("[%lu] ⚠️ Aborting ramp #%lu\n", startTime, rampCounter-1);
    overrideRamping = false;
  }

  overrideMode = mode;
  saveOverrideModeNow();
  float start = currentPercent;
  float end = overrideIntensity(mode);
  if (mode == 0) {
    targetPercent = calculateTargetIntensity();
    overrideRamping = false;
    Serial.printf("[%lu] 🔄 Auto mode, target=%.0f%%\n", millis(), targetPercent);
    return;
  }
  overrideRampStart = start;
  overrideRampEnd = end;
  overrideRampStartTime = millis();
  lastRampProgressTime = millis();
  overrideRamping = true;
  targetPercent = start;
  updatePWM();
  Serial.printf("[%lu] 🔄 Ramp #%lu: %.0f%% → %.0f%% (%ds)\n",
                millis(), rampCounter, start, end, OVERRIDE_RAMP_DURATION);
}

void updateOverrideRamp() {
  if (!overrideRamping) return;
  unsigned long now = millis();
  float elapsed = (now - overrideRampStartTime) / 1000.0;
  float progress = constrain(elapsed / OVERRIDE_RAMP_DURATION, 0.0, 1.0);
  targetPercent = overrideRampStart + (overrideRampEnd - overrideRampStart) * progress;

  static unsigned long lastProgressLog = 0;
  if (now - lastProgressLog > 1000) {
    lastProgressLog = now;
    Serial.printf("[%lu] 📊 Ramp #%lu: %.0f%%\n", now, rampCounter, targetPercent);
  }

  if (progress >= 1.0) {
    targetPercent = overrideRampEnd;
    overrideRamping = false;
    Serial.printf("[%lu] ✅ Ramp #%lu complete: %.0f%%\n", now, rampCounter, targetPercent);
  }
}

// -------------------- Serial Command Handler --------------------
void handleSerialCommands() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == '1') { Serial.println("Serial: ON"); startOverrideRamp(1); }
    else if (c == '0') { Serial.println("Serial: OFF"); startOverrideRamp(2); }
    else if (c == 'a' || c == 'A') { Serial.println("Serial: AUTO"); startOverrideRamp(0); }
  }
}

// -------------------- Web Handlers --------------------
void handleStatus() {
  time_t now = time(nullptr);
  char timeStr[20] = "--:--:--";
  int activeSlot = -1;

  if (now > 10000 && timeSynced) {
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
    int currentSeconds = timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec;

    for (int i = 0; i < NUM_SLOTS; i++) {
      if (!slots[i].enabled) continue;
      int start = timeToSeconds(slots[i].startHour, slots[i].startMinute, 0);
      int end = timeToSeconds(slots[i].endHour, slots[i].endMinute, 0);
      bool contains = end > start
        ? (currentSeconds >= start && currentSeconds < end)
        : (end < start && (currentSeconds >= start || currentSeconds < end));
      if (contains) {
        activeSlot = i + 1;
        break;
      }
    }
  }

  String activeSlotText = activeSlot < 0 ? "None" : String(activeSlot);
  String json = "{\"intensity\":" + String(currentPercent, 1) +
                ",\"currentTime\":\"" + String(timeStr) +
                "\",\"activeSlot\":\"" + activeSlotText + "\"}";
  server.sendHeader("Connection", "close");
  server.send(200, "application/json", json);
}

void handleRoot() {
  time_t now = time(nullptr);
  char timeStr[20] = "--:--:--";
  int activeSlot = -1;
  if (now > 10000 && timeSynced) {
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
    int sec = timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec;
    for (int i = 0; i < NUM_SLOTS; i++) {
      if (!slots[i].enabled) continue;
      int start = timeToSeconds(slots[i].startHour, slots[i].startMinute, 0);
      int end = timeToSeconds(slots[i].endHour, slots[i].endMinute, 0);
      bool contains = false;
      if (end > start) contains = (sec >= start && sec < end);
      else if (end < start) contains = (sec >= start || sec < end);
      if (contains) { activeSlot = i+1; break; }
    }
  }
  String slotLabel = "None";
  if (overrideMode != OVERRIDE_AUTO) slotLabel = "Manual " + String(overrideLabel(overrideMode));
  else if (activeSlot > 0) slotLabel = "Slot " + String(activeSlot);

  String rows = "";
  for (int i = 0; i < NUM_SLOTS; i++) {
    if (!slots[i].enabled) continue;
    char startBuf[6], endBuf[6];
    sprintf(startBuf, "%02d:%02d", slots[i].startHour, slots[i].startMinute);
    sprintf(endBuf, "%02d:%02d", slots[i].endHour, slots[i].endMinute);
    rows += "<tr><td>" + String(i+1) + "</td>";
    rows += "<td>" + String(startBuf) + "</td>";
    rows += "<td>" + String(endBuf) + "</td>";
    rows += "<td>" + String(modeNames[slots[i].mode]) + "</td>";
    rows += "<td>" + String(slots[i].intensity) + "</td></tr>";
  }
  if (rows.length() == 0) rows = "<tr><td colspan='5'>No slots enabled</td></tr>";

  String html = String(FPSTR(index_html));
  html.replace("{intensity}", String((int)round(currentPercent)));
  html.replace("{mode}", String(overrideLabel(overrideMode)));
  html.replace("{time}", String(timeStr));
  html.replace("{slot}", slotLabel);

  server.sendHeader("Connection", "close");
  server.send(200, "text/html", html);
}

void handleSet() {
  if (server.hasArg("mode")) {
    int mode = server.arg("mode").toInt();
    if (mode >= 0 && mode <= 8) {
      startOverrideRamp((uint8_t)mode);
    }
  }
  server.sendHeader("Location", "/");
  server.sendHeader("Connection", "close");
  server.send(303);
}

void handleReset() {
  server.sendHeader("Connection", "close");
  server.send(200, "text/plain", "Resetting...");
  delay(100);
  ESP.restart();
}

void handleSlotsGet() {
  String json = "[";
  for (int i = 0; i < NUM_SLOTS; i++) {
    if (i > 0) json += ",";
    char startBuf[6], endBuf[6];
    sprintf(startBuf, "%02d:%02d", slots[i].startHour, slots[i].startMinute);
    sprintf(endBuf, "%02d:%02d", slots[i].endHour, slots[i].endMinute);
    json += "{\"startTime\":\"" + String(startBuf) + "\",";
    json += "\"endTime\":\"" + String(endBuf) + "\",";
    json += "\"mode\":\"" + String(modeNames[slots[i].mode]) + "\",";
    json += "\"intensity\":" + String(slots[i].intensity) + "}";
  }
  json += "]";
  server.sendHeader("Connection", "close");
  server.send(200, "application/json", json);
}

void handleSlotsPost() {
  if (!server.hasArg("plain")) {
    server.sendHeader("Connection", "close");
    server.send(400, "text/plain", "Empty");
    return;
  }
  String body = server.arg("plain");
  int slotIndex = 0;
  int searchPos = 0;
  while (slotIndex < NUM_SLOTS && searchPos < body.length()) {
    int stPos = body.indexOf("\"startTime\":\"", searchPos);
    if (stPos < 0) break;
    int stEnd = body.indexOf("\"", stPos + 13);
    if (stEnd < 0) break;
    String st = body.substring(stPos + 13, stEnd);
    int sh, sm; sscanf(st.c_str(), "%d:%d", &sh, &sm);

    int enPos = body.indexOf("\"endTime\":\"", stEnd);
    if (enPos < 0) break;
    int enEnd = body.indexOf("\"", enPos + 11);
    if (enEnd < 0) break;
    String en = body.substring(enPos + 11, enEnd);
    int eh, em; sscanf(en.c_str(), "%d:%d", &eh, &em);

    int moPos = body.indexOf("\"mode\":\"", enEnd);
    if (moPos < 0) break;
    int moEnd = body.indexOf("\"", moPos + 8);
    if (moEnd < 0) break;
    String mo = body.substring(moPos + 8, moEnd);
    int mode = 2;
    for (int j = 0; j < 3; j++) if (mo == modeNames[j]) { mode = j; break; }

    int inPos = body.indexOf("\"intensity\":", moEnd);
    if (inPos < 0) break;
    int inEnd = body.indexOf(",", inPos + 12);
    if (inEnd < 0) inEnd = body.indexOf("}", inPos + 12);
    if (inEnd < 0) break;
    String in = body.substring(inPos + 12, inEnd);
    in.trim();
    int inten = in.toInt();

    newSlots[slotIndex].startHour = sh;
    newSlots[slotIndex].startMinute = sm;
    newSlots[slotIndex].endHour = eh;
    newSlots[slotIndex].endMinute = em;
    newSlots[slotIndex].mode = mode;
    newSlots[slotIndex].intensity = constrain(inten, 0, 100);
    newSlots[slotIndex].enabled = !isSlotDisabled(sh, sm, eh, em);
    slotIndex++;
    searchPos = inEnd + 1;
  }
  for (int i = 0; i < NUM_SLOTS; i++) {
    slots[i] = newSlots[i];
  }
  slotsNeedSave = true;
  bool hasEnabledSlot = false;
  for (int i = 0; i < NUM_SLOTS; i++) {
    if (slots[i].enabled) {
      hasEnabledSlot = true;
      break;
    }
  }
  if (!hasEnabledSlot) {
    overrideMode = 0;
    saveOverrideModeNow();
    lastScheduledIntensity = AUTO_FALLBACK;
    targetPercent = AUTO_FALLBACK;
    currentPWMValue = 0.0;
    currentPercent = 0.0;
    setPWM(0);
  }
  if (overrideMode == 0) {
    targetPercent = calculateTargetIntensity();
  }
  server.sendHeader("Connection", "close");
  server.send(200, "text/plain", "Slots saved");
}

void handleMode() {
  if (server.hasArg("mode")) {
    int mode = server.arg("mode").toInt();
    if (mode >= 0 && mode <= 8) {
      startOverrideRamp((uint8_t)mode);
      char buf[80];
      sprintf(buf, "{\"mode\":%d,\"label\":\"%s\"}", overrideMode, overrideLabel(overrideMode));
      server.sendHeader("Connection", "close");
      server.send(200, "application/json", buf);
      return;
    }
    server.sendHeader("Connection", "close");
    server.send(400, "text/plain", "Invalid");
    return;
  }
  char buf[80];
  sprintf(buf, "{\"mode\":%d,\"label\":\"%s\"}", overrideMode, overrideLabel(overrideMode));
  server.sendHeader("Connection", "close");
  server.send(200, "application/json", buf);
}

// -------------------- NTP & Timezone --------------------
void setTimezone() {
  setenv("TZ", TIMEZONE, 1);
  tzset();
}

void syncNTP() {
  if (!wifiConnected) return;
  configTime(0, 0, "pool.ntp.org");
  lastNtpSync = millis();
}

void checkNTP() {
  if (!wifiConnected) return;
  time_t now = time(nullptr);
  if (!timeSynced && now > 10000) {
    timeSynced = true;
    setTimezone();
    lastNtpSync = millis();
    Serial.printf("[%lu] ⏰ NTP sync OK\n", millis());
    return;
  }
  if (timeSynced && (millis() - lastNtpSync) / 1000 >= NTP_SYNC_INTERVAL) {
    Serial.printf("[%lu] ⏰ NTP re-sync\n", millis());
    configTime(0, 0, "pool.ntp.org");
    lastNtpSync = millis();
    setTimezone();
  }
}

// -------------------- WiFi --------------------
bool connectWiFi() {
  Serial.printf("\n📶 Connecting to %s", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < WIFI_TIMEOUT) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("✅ IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
  }
  Serial.printf("❌ WiFi failed (%d)\n", WiFi.status());
  return false;
}

// -------------------- Setup --------------------
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n========================================");
  Serial.println(" Aquarium Light");
  Serial.println("Serial: 1=On, 0=Off, a=Auto");
  Serial.println("========================================");

  pinMode(PWM_PIN, OUTPUT);
  digitalWrite(PWM_PIN, HIGH); delay(100); digitalWrite(PWM_PIN, LOW); delay(100);
  digitalWrite(PWM_PIN, HIGH); delay(100); digitalWrite(PWM_PIN, LOW);
  Serial.println("✅ PWM pin toggled 3 times.");

  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(PWM_PIN, PWM_CHANNEL);
  ledcWrite(PWM_CHANNEL, 255);

  currentPWMValue = 0.0;
  currentPercent = 0.0;
  targetPercent = 0.0;
  startupTime = millis();

  loadOverrideMode();
  loadSlots();

  wifiConnected = connectWiFi();
  if (wifiConnected) {
    syncNTP();
  }

  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.on("/reset", handleReset);
  server.on("/api/status", handleStatus);
  server.on("/api/mode", HTTP_GET, handleMode);
  server.on("/api/mode", HTTP_POST, handleMode);
  server.on("/api/slots", HTTP_GET, handleSlotsGet);
  server.on("/api/slots", HTTP_POST, handleSlotsPost);
  server.begin();
  Serial.println("✅ Web server started");
  if (wifiConnected) Serial.printf("🌐 http://%s\n", WiFi.localIP().toString().c_str());
  Serial.println("========================================\n");

  // Initial PWM
  if (overrideMode == OVERRIDE_AUTO) targetPercent = calculateTargetIntensity();
  else targetPercent = overrideIntensity(overrideMode);
  if (overrideMode == 0) {
    setPWM(0);
    currentPWMValue = 0.0;
    currentPercent = 0.0;
  } else {
    setPWM((uint8_t)((targetPercent / 100.0) * 255.0));
    currentPWMValue = (targetPercent / 100.0) * 255.0;
    currentPercent = targetPercent;
  }
}

// -------------------- Loop --------------------
void loop() {
  static unsigned long lastHeartbeat = 0;
  static unsigned long lastLog = 0;

  // Serial commands
  handleSerialCommands();

  // Handle web server - run every loop iteration
  server.handleClient();

  // NTP check (non-blocking)
  static unsigned long lastNtpCheck = 0;
  if (millis() - lastNtpCheck > 5000) {
    lastNtpCheck = millis();
    checkNTP();
  }

  // Save slots if needed
  if (slotsNeedSave) {
    saveSlotsNow();
  }

  // Update ramps
  if (overrideRamping) {
    updateOverrideRamp();
  } else if (overrideMode == 0) {
    float newTarget = calculateTargetIntensity();
    targetPercent = newTarget;
    static float lastLoggedTarget = -1;
    if (abs(newTarget - lastLoggedTarget) > 1.0) {
      lastLoggedTarget = newTarget;
      Serial.printf("[%lu] 🔄 Auto target: %.0f%%\n", millis(), targetPercent);
    }
  }

  // PWM update every 50ms
  if (millis() - lastUpdateTime >= UPDATE_INTERVAL) {
    lastUpdateTime = millis();
    updatePWM();
  }

  // Heartbeat every 2s
  if (millis() - lastHeartbeat > 2000) {
    lastHeartbeat = millis();
    Serial.print(".");
  }

  // Status log every 10s
  if (millis() - lastLog > 10000) {
    lastLog = millis();
    Serial.printf("\n[%lu] Current: %.0f%%, Target: %.0f%%, Mode: %s\n",
                  millis(), currentPercent, targetPercent,
                  overrideLabel(overrideMode));
  }

  // Small yield to prevent watchdog
  delay(1);
}