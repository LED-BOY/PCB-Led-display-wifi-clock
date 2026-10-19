#include "WiFiManager.h"
#include "CharlieDisplay.h"
#include <esp_sntp.h>
#include <nvs_flash.h>
#include <atomic>
#include <stdlib.h>
#include <math.h>
#include <NetworkClientSecure.h>

namespace {
std::atomic<bool> ntpSyncReceived{false};

void onNtpTimeSync(struct timeval*) {
  ntpSyncReceived = true;
}

int sanitizeTimezoneOffsetMinutes(int offsetMinutes) {
  return offsetMinutes >= -14 * 60 && offsetMinutes <= 14 * 60
      ? offsetMinutes
      : DEFAULT_TIMEZONE_OFFSET * 60;
}

String htmlEscape(const String& value) {
  String escaped;
  escaped.reserve(value.length());

  for (size_t i = 0; i < value.length(); ++i) {
    switch (value[i]) {
      case '&': escaped += "&amp;"; break;
      case '<': escaped += "&lt;"; break;
      case '>': escaped += "&gt;"; break;
      case '\"': escaped += "&quot;"; break;
      case '\'': escaped += "&#39;"; break;
      default: escaped += value[i]; break;
    }
  }

  return escaped;
}

String scanNetworkOptions(const String& selectedSsid, bool& selectedFound,
                          bool* scanSucceeded = nullptr) {
  String options;
  options.reserve(2048);
  selectedFound = false;

  const int networkCount = WiFi.scanNetworks();
  if (scanSucceeded) *scanSucceeded = networkCount >= 0;
  for (int i = 0; i < networkCount; ++i) {
    const String networkSsid = WiFi.SSID(i);
    if (networkSsid.isEmpty()) continue;

    bool duplicate = false;
    for (int j = 0; j < i; ++j) {
      if (networkSsid == WiFi.SSID(j)) {
        duplicate = true;
        break;
      }
    }
    if (duplicate) continue;

    const bool selected = networkSsid == selectedSsid;
    if (selected) selectedFound = true;
    const int rssi = WiFi.RSSI(i);
    const char* signalStrength = rssi >= -55 ? "[####] Excellent"
        : rssi >= -67 ? "[###-] Good"
        : rssi >= -75 ? "[##--] Fair"
        : "[#---] Weak";
    options += "<option value='" + htmlEscape(networkSsid) + "'";
    if (selected) options += " selected";
    options += ">" + htmlEscape(networkSsid) + " " + signalStrength + "</option>";
  }
  WiFi.scanDelete();
  return options;
}
}

WiFiManager wifi;

WiFiManager::WiFiManager() {}

// ===============================
// 📦 LOAD PREFS CACHE
// ===============================
void WiFiManager::loadPrefs() {

  if (prefsLoaded) return;

  prefs.begin("wifi", true);

  syncTimeCached = prefs.getBool("syncTime", true);
  showTempCached = prefs.getBool("showTemp", false);
  brightnessCached = prefs.getInt("brightness", DEFAULT_BRIGHTNESS);
  if (brightnessCached != AUTO_BRIGHTNESS_LOW &&
      brightnessCached != AUTO_BRIGHTNESS_HIGH) {
    brightnessCached = DEFAULT_BRIGHTNESS;
  }
  autoBrightnessCached = prefs.getBool("autoBrightness", false);
  boostModeCached = prefs.getBool("boostMode", false);

  prefs.end();

  prefsLoaded = true;
}

// ===============================
// 🔧 HELPERS
// ===============================
void WiFiManager::setNetworkBudget(uint32_t durationMs) {
  networkBudgetStart = millis();
  networkBudgetMs = durationMs;
}

uint32_t WiFiManager::remainingNetworkTime() const {
  const uint32_t elapsed = millis() - networkBudgetStart;
  return elapsed >= networkBudgetMs ? 0 : networkBudgetMs - elapsed;
}

bool WiFiManager::httpGET(const String& url, String& response) {
  response = "";
  uint32_t remaining = remainingNetworkTime();
  if (remaining < 100) return false;
  NetworkClient plainClient;
  NetworkClientSecure secureClient;
  // Keep the existing TLS trust policy, but bound its handshake explicitly.
  secureClient.setInsecure();
  secureClient.setHandshakeTimeout(remaining >= 3000 ? 3 : 1);
  HTTPClient http;
  // HTTP/1.0 requests a body terminated by its length or connection close,
  // allowing explicit deadline checks instead of blocking getString().
  http.useHTTP10(true);
  NetworkClient& client = url.startsWith("https://")
      ? static_cast<NetworkClient&>(secureClient) : plainClient;
  if (!http.begin(client, url)) return false;
  const char* headers[] = {"Transfer-Encoding"};
  http.collectHeaders(headers, 1);
  const uint32_t timeout = remaining < 2500 ? remaining : 2500;
  http.setConnectTimeout(timeout);
  http.setTimeout(timeout);
  http.setReuse(false);
  const int status = http.GET();
  const int expected = http.getSize();
  // Do not interpret chunk framing or an unbounded response as weather JSON.
  if (status != 200 || expected > 8192 ||
      http.header("Transfer-Encoding").equalsIgnoreCase("chunked")) {
    http.end();
    return false;
  }

  auto* stream = http.getStreamPtr();
  if (!stream) {
    http.end();
    return false;
  }
  bool complete = false;
  uint32_t lastData = millis();
  while (remainingNetworkTime() > 0) {
    if (expected >= 0 && response.length() == static_cast<size_t>(expected)) {
      complete = true;
      break;
    }
    const int available = stream->available();
    if (available > 0) {
      uint8_t buffer[256];
      size_t count = available < 256 ? available : 256;
      if (expected >= 0 && count > static_cast<size_t>(expected) - response.length()) {
        count = expected - response.length();
      }
      const int received = stream->read(buffer, count);
      if (received > 0) {
        if (response.length() + received > 8192 ||
            !response.concat(reinterpret_cast<const char*>(buffer), received)) break;
        lastData = millis();
      }
    } else if (!http.connected()) {
      complete = expected < 0 || response.length() == static_cast<size_t>(expected);
      break;
    } else if (millis() - lastData >= timeout) {
      break;
    }
    delay(1);
  }
  http.end();
  if (!complete || response.isEmpty()) response = "";
  return complete && !response.isEmpty();
}

// ===============================
// 📶 WIFI
// ===============================
bool WiFiManager::connect(uint32_t timeoutMs) {

  // Manual-time mode is intentionally offline, even with saved credentials.
  if (!isTimeSyncEnabled()) return false;

  prefs.begin("wifi", true);
  String ssid = prefs.getString("ssid", "");
  String password = prefs.getString("password", "");
  prefs.end();

  if (ssid.isEmpty()) return false;

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start >= timeoutMs || remainingNetworkTime() == 0) return false;
    delay(25);
  }
  return true;
}

bool WiFiManager::tryReconnect(uint32_t timeoutMs) {
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
  if (isConnected()) return true;
  return connect(timeoutMs);
}

// ===============================
// 🌐 PORTAL
// ===============================
void WiFiManager::startPortalAsync() {

  configured = false;
  displayTestActive = false;
  displayTestBrightness = DEFAULT_BRIGHTNESS;

  prefs.begin("wifi", true);
  String ssidSaved = prefs.getString("ssid", "");
  String passwordSaved = prefs.getString("password", "");
  int offsetSaved = prefs.getInt("offset", DEFAULT_TIMEZONE_OFFSET);
  int offsetMode = prefs.getInt("offsetMode", 1);
  const int safeOffsetHours = offsetSaved >= -14 && offsetSaved <= 14
      ? offsetSaved : DEFAULT_TIMEZONE_OFFSET;
  const int offsetMinutesSaved = prefs.getInt(
      "offsetMinutes", safeOffsetHours * 60);
  prefs.end();

  cachedOffsetMinutes = sanitizeTimezoneOffsetMinutes(
      offsetMode == 1 ? offsetMinutesSaved : safeOffsetHours * 60);
  timezoneLoaded = true;
  autoTimezoneNeedsDetection = offsetMode == 1;

  loadPrefs();  // Load once; subsequent accesses use the in-memory cache.

  // Finish scanning before exposing the AP, so its first captive-portal
  // request can be answered immediately.
  WiFi.mode(WIFI_STA);

  bool savedNetworkFound = false;
  const String networkOptions = scanNetworkOptions(ssidSaved, savedNetworkFound);

  server.on("/scan", HTTP_GET, [this]() {
    bool ignored = false;
    bool scanSucceeded = false;
    const String options = scanNetworkOptions("", ignored, &scanSucceeded);
    server.sendHeader("Cache-Control", "no-store");
    if (scanSucceeded) {
      server.send(200, "text/html", options);
    } else {
      server.send(503, "text/plain", "Wi-Fi scan failed.");
    }
  });

  server.on("/cancel", HTTP_POST, [this]() {
    stopDisplayTest();
    server.sendHeader("Connection", "close");
    server.send(200, "text/html",
        "<html><body><h2>Setup canceled. Starting clock with saved settings...</h2></body></html>");
    delay(800);  // Let the browser receive the page before the AP is closed.
    configured = true;
  });
  server.on("/update", HTTP_GET, [this]() {
    stopDisplayTest();
    String html;
    html.reserve(2048);
    html = R"====(
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body { font-family: Arial; padding:20px; }
button { padding:10px 20px; font-size:16px; margin-right:8px; }
button:disabled { background:#aaa; color:#666; cursor:not-allowed; }
input { margin:8px 0; }
.file-input {
  position:absolute;
  width:1px;
  height:1px;
  opacity:0;
}
.file-button {
  display:inline-block;
  padding:4px 12px;
  margin:8px 8px 8px 0;
  border:1px solid #888;
  background:#eee;
  cursor:pointer;
}
</style>
</head>
<body>
<h2>Firmware Update</h2>
)====";

    html += "<p>FW:";
    html += FIRMWARE_VERSION;
    html += "</p>";

    html += R"====(
<form id='updateForm' method='POST' action='/update' enctype='multipart/form-data'>
  Select firmware file:<br>
  <input class='file-input' id='firmwareFile' type='file' name='update' accept='.bin' required>
  <label class='file-button' for='firmwareFile'>Choose File</label>
  <span id='fileName'>No file chosen</span>
  <br><br>
  <button id='updateButton' type='submit'>Upload Firmware</button>
  <button type='button' onclick="window.location.href='/'">Back to Setup</button>
</form>
<script>
document.getElementById('firmwareFile').addEventListener('change', function() {
  document.getElementById('fileName').textContent =
    this.files.length ? this.files[0].name : 'No file chosen';
});
document.getElementById('updateForm').addEventListener('submit', function() {
  const button = document.getElementById('updateButton');
  button.disabled = true;
  button.textContent = 'Uploading...';
});
</script>
</body>
</html>
)====";

    server.send(200, "text/html", html);
  });

  server.on("/manual", HTTP_GET, [this]() {
    stopDisplayTest();
    String html;
    html.reserve(2048);
    html = R"====(
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body { font-family: Arial; padding:20px; line-height:1.45; }
button { padding:10px 20px; font-size:16px; }
h2 { margin-top:0; }
h3 { margin-bottom:4px; }
p { margin-top:4px; }
</style>
</head>
<body>
<h2>Portal Manual</h2>

<h3>Wi-Fi Setup</h3>
<p>Enter your Wi-Fi network name (SSID) and password so the clock can connect to the internet. Only 2.4 GHz Wi-Fi networks are compatible.</p>
<p>After joining the ClockSetup access point, the setup page should appear automatically. If it does not, open http://192.168.4.1/ in a browser.</p>
<p>Use <b>Rescan Wi-Fi</b> to refresh the available networks without losing the settings currently entered on the page.</p>

<h3>Time zone</h3>
<p>Select <b>Auto (IP)</b> to detect the time zone from your internet connection, or choose a fixed UTC offset.</p>

<h3>Auto sync time</h3>
<p>When enabled, the clock sets its time from the internet. Turn it off to use <b>Manual time</b>; enter the time as HHMM, for example 0930.</p>

<h3>Show temperature</h3>
<p>When automatic time sync is enabled, this option shows the temperature on the clock display.</p>

<h3>Brightness</h3>
<p>Enable <b>Auto brightness</b> to let the clock adjust automatically. With automatic time sync enabled, the clock uses dawn and sunset data from the server. Otherwise, it uses the default night schedule from 20:00 to 08:00. Turn Auto brightness off to choose Low or High brightness yourself.</p>

<h3>Boost brightness</h3>
<p>Enable <b>Boost brightness</b> to raise the High brightness level from 240 to its maximum value of 255. It applies when High brightness is selected manually and during automatic High-brightness periods. Low brightness remains unchanged.</p>

<h3>Save</h3>
<p>Press Save to store the settings. The clock reboots after saving.</p>

<h3>Cancel</h3>
<p>Press Cancel to discard changes on this page and continue booting with the saved settings.</p>

<h3>Firmware Update</h3>
<p>Use this page to select and upload a compatible .bin firmware file, then wait for the update to finish.</p>

<h3>Display Test</h3>
<p>Open Display Test to light every segment and the colon at the default brightness. Enter a temporary value from 0 to 255 to check other levels. The test value is never saved.</p>

<h3>BOOT button</h3>
<p>Press the BOOT button while the clock is running to force an immediate Wi-Fi and time synchronization.</p>

<h3>Synchronization</h3>
<p>The clock keeps updating during background synchronization. Wi-Fi connection waits up to 20 seconds; time synchronization has a separate 10-second window with backup servers. Failed time or weather updates retry after 1, 5, then 15 minutes. Successful services refresh hourly. A weather-only retry does not repeat time synchronization.</p>
<p>The colon stays steady during network work, retaining the previous Wi-Fi result. When the attempt finishes, it turns on if Wi-Fi connected or off if the connection failed. A lit colon does not guarantee that time or weather updated successfully.</p>

<br><button type='button' onclick="window.location.href='/'">Back to Setup</button>
</body>
</html>
)====";

    server.send(200, "text/html", html);
  });

  server.on("/test", HTTP_GET, [this]() {
    if (!displayTestActive) {
      displayTestBrightness = DEFAULT_BRIGHTNESS;
      display.setBrightness(displayTestBrightness);
      display.setColonBrightness(displayTestBrightness);
      display.setColonBlink(false);
      display.print("8888");  // Every segment in all four digits.
      display.setColon(true);
      displayTestActive = true;
    }

    String html;
    html.reserve(1400);
    html = R"====(
<html><head><meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body { font-family:Arial; padding:20px; line-height:1.45; }
input { width:100%; max-width:120px; padding:8px; font-size:16px; }
button { padding:10px 20px; font-size:16px; margin:8px 8px 8px 0; }
</style></head><body>
<h2>Display Test</h2>
<p>All digit segments and the colon are on. Brightness is temporary and is not saved.</p>
<p>Very low brightness values can make segments or the colon too dim to see. Increase the value if an LED appears off.</p>
<form action='/test/brightness' method='POST'>
<label for='level'>Test brightness (0-255):</label><br>
<input id='level' name='level' type='number' min='0' max='255' step='1' required value=')====";
    html += String(displayTestBrightness);
    html += R"====('><br><button type='submit'>Apply Brightness</button>
</form>
<form action='/test/stop' method='POST'>
<button type='submit'>Stop Test &amp; Back to Setup</button>
</form>
</body></html>
)====";
    server.send(200, "text/html", html);
  });

  server.on("/test/brightness", HTTP_POST, [this]() {
    const String level = server.arg("level");
    if (level.isEmpty() || level.length() > 3) {
      server.send(400, "text/plain", "Brightness must be an integer from 0 to 255.");
      return;
    }
    for (size_t i = 0; i < level.length(); ++i) {
      if (level[i] < '0' || level[i] > '9') {
        server.send(400, "text/plain", "Brightness must be an integer from 0 to 255.");
        return;
      }
    }
    const int value = level.toInt();
    if (value > 255) {
      server.send(400, "text/plain", "Brightness must be an integer from 0 to 255.");
      return;
    }

    displayTestBrightness = static_cast<uint8_t>(value);
    display.setBrightness(displayTestBrightness);
    display.setColonBrightness(displayTestBrightness);
    display.setColonBlink(false);
    display.print("8888");
    display.setColon(true);
    displayTestActive = true;
    server.sendHeader("Location", "/test");
    server.send(303, "text/plain", "");
  });

  server.on("/test/stop", HTTP_POST, [this]() {
    stopDisplayTest();
    server.sendHeader("Location", "/");
    server.send(303, "text/plain", "");
  });

  server.on("/", [=]() {
    // Captive-portal probes also request this page in the background.
    // Keep the test running until an explicit stop or portal exit.
    String html;
    html.reserve(8192);
    html = R"====(
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body { font-family: Arial; padding:20px; }
input:not([type='checkbox']), select {
  width:100%;
  max-width:300px;
  padding:8px;
  margin:8px 0;
}
input[readonly] {
  background:#eee;
  color:#666;
}

input[type='checkbox'] {
  width:auto;
  max-width:none;
  padding:0;
  margin-left:8px;
  transform:scale(1.1);
}

button { padding:10px 20px; font-size:16px; }
button.reset-button { margin-top:16px; background:#b00020; color:#fff; border:0; }
.portal-link {
  display:inline-block;
  min-width:180px;
  padding:12px 16px;
  margin:8px 0;
  border:1px solid #666;
  border-radius:4px;
  background:#eee;
  color:#111;
  font-size:16px;
  text-align:center;
  text-decoration:none;
}
)====";

    if (syncTimeCached) {
      html += "#manualBlock { display:none; }";
    } else {
      html += "#tempBlock { display:none; }";
    }

    if (autoBrightnessCached) {
      html += "#brightnessBlock { display:none; }";
    }

    html += R"====(
</style>
</head>
<body>
)====";


    html += R"====(
<h2>Wi-Fi Setup</h2>

<form id='setupForm' action='/save' method='POST'>
)====";

    html += "SSID:<br><select id='ssidSelect'>";
    html += "<option value='__manual__'";
    if (ssidSaved.isEmpty()) html += " selected";
    html += ">Enter SSID manually...</option>";
    if (!ssidSaved.isEmpty() && !savedNetworkFound) {
      html += "<option value='" + htmlEscape(ssidSaved) + "' selected>";
      html += htmlEscape(ssidSaved) + " (saved; not detected)</option>";
    }
    html += networkOptions;
    html += "</select><br>";
    html += "<button id='rescanButton' type='button'>Rescan Wi-Fi</button><br>";
    html += "<input id='ssid' name='ssid' placeholder='Wi-Fi network name (optional)' value='";
    html += htmlEscape(ssidSaved) + "'><br>";
    html += "Password:<br><input name='password' type='password' value='" + passwordSaved + "'><br><br>";

    html += "Time zone:<br><select id='offset' name='offset'>";

    html += "<option value='auto' ";
    if (offsetMode == 1) html += "selected";
    html += ">Auto (IP)</option>";

    int offs[] = { -8, -5, -3, 0, 1, 2, 3, 5, 8, 9, 10 };
    const char* labels[] = {
      "UTC-8 LA", "UTC-5 NY", "UTC-3 Uruguay",
      "UTC+0 London", "UTC+1 EU", "UTC+2 Cairo",
      "UTC+3 Moscow", "UTC+5 Karachi", "UTC+8 China",
      "UTC+9 Japan", "UTC+10 Sydney"
    };

    for (int i = 0; i < 11; i++) {
      html += "<option value='" + String(offs[i]) + "'";
      if (offsetMode == 0 && offsetSaved == offs[i]) html += " selected";
      html += ">" + String(labels[i]) + "</option>";
    }

    html += "</select><br><br>";

    html += "Auto sync time:<input type='checkbox' id='syncTime' name='syncTime' ";
    if (syncTimeCached) html += "checked";
    html += "><br><br>";

    html += "<div id='manualBlock'>";
    html += "Manual time (HHMM):<br>";
    html += "<input id='manualTime' name='manualTime' type='text' inputmode='numeric' maxlength='4' placeholder='HHMM' autocomplete='off'>";
    html += "<div id='manualTimeError' style='display:none;color:#b00020;margin-top:4px'>Valid values are from 0000 to 2359.</div><br><br>";
    html += "</div>";

    html += "<div id='tempBlock'>";
    html += "Show temperature:<input type='checkbox' name='showTemp' ";
    if (showTempCached) html += "checked";
    html += "><br><br>";
    html += "</div>";

    html += "<div id='autoBrightnessBlock'>";
    html += "Auto brightness:<input type='checkbox' id='autoBrightness' name='autoBrightness' ";
    if (autoBrightnessCached) html += "checked";
    html += "><br><br></div>";

    html += "Boost brightness:<input type='checkbox' name='boostMode' ";
    if (boostModeCached) html += "checked";
    html += "><br><br>";

    html += "<div id='brightnessBlock'>";
    html += "Brightness:<br><select name='brightness'>";

    html += "<option value='" + String(AUTO_BRIGHTNESS_LOW) + "' ";
    if (brightnessCached == AUTO_BRIGHTNESS_LOW) html += "selected";
    html += ">Low</option>";

    html += "<option value='" + String(AUTO_BRIGHTNESS_HIGH) + "' ";
    if (brightnessCached == AUTO_BRIGHTNESS_HIGH) html += "selected";
    html += ">High</option>";

    html += "</select><br><br>";
    html += "</div>";

    html += "<button id='saveButton' type='submit'>Save</button> ";
    html += "<button type='submit' form='cancelForm' formnovalidate>Cancel</button></form>";
    html += "<form id='cancelForm' action='/cancel' method='POST'></form>";

    html += R"====(
<form action='/reset' method='POST'
      onsubmit="return confirm('Reset all saved settings? This permanently removes Wi-Fi credentials and every other NVS setting.');">
  <button class='reset-button' type='submit'>Reset to Defaults</button>
</form>
)====";

    html += "<br><br><a class='portal-link' href='/manual'>Portal Manual</a>";
    html += "<br><br><a class='portal-link' href='/update'>Firmware Update</a>";
    html += "<br><br><a class='portal-link' href='/test'>Display Test</a>";

    html += R"====(
<script>
function toggleManualTime() {
  const sync = document.getElementById('syncTime');
  const manual = document.getElementById('manualBlock');
  const temp = document.getElementById('tempBlock');
  const offset = document.getElementById('offset');
  const auto = document.getElementById('autoBrightness');
  const autoBlock = document.getElementById('autoBrightnessBlock');

  if (!sync) return;

  if (manual) manual.style.display = sync.checked ? 'none' : 'block';
  if (temp) temp.style.display = sync.checked ? 'block' : 'none';
  if (autoBlock) autoBlock.style.display = sync.checked ? 'block' : 'none';
  if (auto) auto.disabled = !sync.checked;
  if (offset) {
    offset.disabled = !sync.checked;
    offset.style.opacity = sync.checked ? '1' : '0.55';
  }
  toggleBrightness();
}

function toggleBrightness() {
  const auto = document.getElementById('autoBrightness');
  const brightness = document.getElementById('brightnessBlock');
  const sync = document.getElementById('syncTime');

  if (!auto || !brightness) return;

  brightness.style.display = (!sync || !sync.checked || !auto.checked)
      ? 'block'
      : 'none';
}

function validateManualTime() {
  const sync = document.getElementById('syncTime');
  const manualTime = document.getElementById('manualTime');
  const error = document.getElementById('manualTimeError');

  if (!manualTime || !error || (sync && sync.checked)) return true;

  const value = manualTime.value.trim();
  // A blank manual time is intentionally saved as 0000 (midnight).
  const valid = value === '' || (/^\d{4}$/.test(value) &&
      Number(value.slice(0, 2)) <= 23 && Number(value.slice(2, 4)) <= 59);

  error.style.display = valid ? 'none' : 'block';
  if (!valid) manualTime.focus();
  return valid;
}

document.addEventListener("DOMContentLoaded", function() {
  const sync = document.getElementById('syncTime');
  const auto = document.getElementById('autoBrightness');
  const form = document.getElementById('setupForm');
  const ssidSelect = document.getElementById('ssidSelect');
  const ssidInput = document.getElementById('ssid');
  const rescanButton = document.getElementById('rescanButton');
  const manualTime = document.getElementById('manualTime');
  const saveButton = document.getElementById('saveButton');

  if (sync) {
    sync.addEventListener('change', toggleManualTime);
    toggleManualTime();
  }

  if (auto) {
    auto.addEventListener('change', toggleBrightness);
    toggleBrightness();
  }

  if (ssidSelect && ssidInput) {
    function updateSsidInput(clearManualValue) {
      const manualEntry = ssidSelect.value === '__manual__';
      ssidInput.readOnly = !manualEntry;
      if (manualEntry && clearManualValue) {
        ssidInput.value = '';
        ssidInput.focus();
      } else if (!manualEntry) {
        ssidInput.value = ssidSelect.value;
      }
    }

    ssidSelect.addEventListener('change', function() {
      updateSsidInput(true);
    });

    updateSsidInput(false);
  }

  if (rescanButton && ssidSelect && ssidInput) {
    rescanButton.addEventListener('click', async function() {
      const selectedValue = ssidSelect.value;
      const typedValue = ssidInput.value;
      const wasManual = selectedValue === '__manual__';
      rescanButton.disabled = true;
      rescanButton.textContent = 'Scanning...';

      try {
        const response = await fetch('/scan', { cache: 'no-store' });
        if (!response.ok) throw new Error('Scan failed');
        const scanned = document.createElement('select');
        scanned.innerHTML = await response.text();

        ssidSelect.innerHTML = '';
        ssidSelect.add(new Option('Enter SSID manually...', '__manual__'));
        while (scanned.options.length) ssidSelect.add(scanned.options[0]);
        if (!wasManual && selectedValue &&
            !Array.from(ssidSelect.options).some(option => option.value === selectedValue)) {
          ssidSelect.add(new Option(selectedValue + ' (not detected)', selectedValue));
        }
        ssidSelect.value = wasManual ? '__manual__' : selectedValue;
        ssidInput.value = typedValue;
        ssidInput.readOnly = !wasManual;
      } catch (error) {
        // Keep the current list if the scan fails; another press can retry.
      } finally {
        rescanButton.disabled = false;
        rescanButton.textContent = 'Rescan Wi-Fi';
      }
    });
  }

  if (form) form.addEventListener('submit', function(event) {
    if (!validateManualTime()) {
      event.preventDefault();
      return;
    }

    if (saveButton) {
      saveButton.disabled = true;
      saveButton.textContent = 'Saving...';
      saveButton.style.backgroundColor = '#999';
      saveButton.style.color = '#eee';
      saveButton.style.cursor = 'wait';
    }
  });

  if (manualTime) manualTime.addEventListener('input', validateManualTime);
});
</script>
)====";

    html += "</body></html>";

    server.send(200, "text/html", html);
  });

  // =========================
  // FACTORY RESET
  // =========================
  server.on("/reset", HTTP_POST, [this]() {
    // Erase the whole NVS partition, not only this sketch's "wifi" namespace.
    // This removes all saved settings, including the Wi-Fi SSID and password.
    server.send(
        200,
        "text/html",
        "<html><body><h2>Settings cleared.</h2>"
        "<p>The clock is restarting into setup mode...</p></body></html>");

    delay(800);  // Allow the browser to receive the confirmation page.
    nvs_flash_erase();
    ESP.restart();
  });

  // =========================
  // SAVE
  // =========================
  server.on("/save", [this]() {
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    String offsetStr = server.arg("offset");
    String manualTime = server.arg("manualTime");

    bool syncTime = server.hasArg("syncTime");
    bool showTemp = server.hasArg("showTemp");
    bool autoBrightness = server.hasArg("autoBrightness");
    bool boostMode = server.hasArg("boostMode");

    // Start with the saved automatic-time preference. Manual mode uses its
    // own working offset and must not overwrite the user's time-zone choice.
    prefs.begin("wifi", true);
    int offset = prefs.getInt("offset", DEFAULT_TIMEZONE_OFFSET);
    int offsetMode = prefs.getInt("offsetMode", 1);
    if (offset < -14 || offset > 14) offset = DEFAULT_TIMEZONE_OFFSET;
    int offsetMinutes = sanitizeTimezoneOffsetMinutes(
        prefs.getInt("offsetMinutes", offset * 60));
    prefs.end();

    if (syncTime) {
      if (offsetStr == "auto") {
        offsetMode = 1;
        // The portal is AP-only. Detect after station Wi-Fi and NTP succeed.
        autoTimezoneNeedsDetection = true;
      } else {
        offset = offsetStr.toInt();
        if (offset < -14 || offset > 14) offset = DEFAULT_TIMEZONE_OFFSET;
        offsetMinutes = offset * 60;
        offsetMode = 0;
        autoTimezoneNeedsDetection = false;
      }
    }

    // Manual time uses DEFAULT_MANUAL_TIMEZONE_OFFSET internally. Preserve
    // the saved automatic-time offset so it resumes when sync is re-enabled.
    if (!syncTime) {
      showTemp = false;
    }

    // In manual mode, an empty field means midnight (0000).
    if (!syncTime && manualTime.length() == 0) {
      manualTime = "0000";
    }

    bool validManualTime = manualTime.length() == 4;
    for (uint8_t i = 0; i < manualTime.length() && validManualTime; i++) {
      validManualTime = manualTime[i] >= '0' && manualTime[i] <= '9';
    }

    int enteredHour = manualTime.substring(0, 2).toInt();
    int enteredMinute = manualTime.substring(2, 4).toInt();
    validManualTime = validManualTime && enteredHour <= 23 && enteredMinute <= 59;

    if (!syncTime && validManualTime) {
      manualHour = enteredHour;
      manualMinute = enteredMinute;
      manualTimeSet = true;
    } else {
      manualTimeSet = false;
    }

    int brightness = server.arg("brightness").toInt();
    if (brightness != AUTO_BRIGHTNESS_LOW &&
        brightness != AUTO_BRIGHTNESS_HIGH) {
      brightness = DEFAULT_BRIGHTNESS;
    }

    prefs.begin("wifi", false);
    prefs.putString("ssid", ssid);
    prefs.putString("password", password);
    prefs.putInt("offset", offset);
    prefs.putInt("offsetMinutes", offsetMinutes);
    prefs.putInt("offsetMode", offsetMode);
    prefs.putInt("brightness", brightness);
    prefs.putBool("syncTime", syncTime);
    prefs.putBool("showTemp", showTemp);
    prefs.putBool("autoBrightness", autoBrightness);
    prefs.putBool("boostMode", boostMode);
    prefs.end();

    // Keep the active portal state aligned with the saved preferences.
    syncTimeCached = syncTime;
    showTempCached = showTemp;
    brightnessCached = brightness;
    autoBrightnessCached = autoBrightness;
    boostModeCached = boostMode;
    cachedOffsetMinutes = offsetMinutes;
    timezoneLoaded = true;
    prefsLoaded = true;

    configured = true;

    server.send(200, "text/html", "<html><body><h2>Saved! Starting clock...</h2></body></html>");

    delay(1200);
    server.stop();
  });

  server.on(
    "/update",
    HTTP_POST,
    [this]() {
      bool ok = otaUpdateOk && !Update.hasError();

      server.sendHeader("Connection", "close");
      if (ok) {
        // A successful OTA install removes all saved user data, including
        // Wi-Fi credentials. The setup portal is shown after the reboot.
        prefs.begin("wifi", false);
        prefs.clear();
        prefs.end();

        server.send(
          200,
          "text/html",
          R"====(<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body { font-family:Arial; padding:20px; }
</style>
</head>
<body>
<h2>Upgrade Successful!</h2>
<p>All saved data, including Wi-Fi SSID and password, has been cleared.</p>
<p>The device will restart automatically.</p>
</body>
</html>)====");
      } else {
        server.send(200, "text/html", "<html><body><h2>Update failed.</h2></body></html>");
      }

      if (ok) {
        // Give the browser time to receive the success response, then boot the
        // freshly written OTA image even if the setup portal is about to time out.
        delay(1500);
        ESP.restart();
      }
    },
    [this]() {
      HTTPUpload& upload = server.upload();

      if (upload.status == UPLOAD_FILE_START) {
        display.print("FWUP");
        display.setColon(false);
        otaUpdateOk = false;
        Update.begin(UPDATE_SIZE_UNKNOWN);
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (!Update.hasError()) {
          Update.write(upload.buf, upload.currentSize);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (!Update.hasError()) {
          otaUpdateOk = Update.end(true);
        }
      }
    });

  // Catch the HTTP connectivity checks used by phones and computers after
  // joining a Wi-Fi network, then send their browser to the setup page.
  server.onNotFound([this]() {
    if (server.method() == HTTP_GET) {
      server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/");
      server.send(302, "text/plain", "");
    } else {
      server.send(404, "text/plain", "Not found");
    }
  });

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("ClockSetup", "12345678");
  delay(500);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 2)
  WiFi.AP.enableDhcpCaptivePortal();
#endif
  dnsServer.start(53, "*", WiFi.softAPIP());
  server.begin();
}

// ===============================
// 🌦 WEATHER
// ===============================
bool WiFiManager::getWeatherAuto(float& tempOut,
                                 int& sunriseMinuteOut, int& sunsetMinuteOut) {
  if (WiFi.status() != WL_CONNECTED) return false;

  if (!weatherLocationCached) {
    String location;
    if (!httpGET("http://ip-api.com/json/?fields=status,lat,lon", location) ||
        location.indexOf("\"status\":\"success\"") < 0) return false;

    weatherLatitude = extractFloat(location, "\"lat\":");
    weatherLongitude = extractFloat(location, "\"lon\":");
    if (isnan(weatherLatitude) || isnan(weatherLongitude) ||
        weatherLatitude < -90.0f || weatherLatitude > 90.0f ||
        weatherLongitude < -180.0f || weatherLongitude > 180.0f) {
      return false;
    }
    weatherLocationCached = true;
  }

  String weather;
  String url =
    "http://api.open-meteo.com/v1/forecast?"
    "latitude="
    + String(weatherLatitude, 4) + "&longitude=" + String(weatherLongitude, 4)
    + "&current=temperature_2m"
      "&daily=sunrise,sunset&timezone=auto&forecast_days=1";

  if (!httpGET(url, weather)) return false;

  int curStart = weather.indexOf("\"current\":{");
  int curEnd = weather.indexOf("}", curStart);
  if (curStart < 0 || curEnd < 0) return false;

  String cur = weather.substring(curStart, curEnd);

  tempOut = extractFloat(cur, "\"temperature_2m\":");
  sunriseMinuteOut = extractIsoTimeMinutes(weather, "\"sunrise\":[\"");
  sunsetMinuteOut = extractIsoTimeMinutes(weather, "\"sunset\":[\"");

  bool validWeather = !isnan(tempOut);
  bool validSunTimes = sunriseMinuteOut >= 0 && sunriseMinuteOut < 1440
      && sunsetMinuteOut >= 0 && sunsetMinuteOut < 1440
      && sunriseMinuteOut < sunsetMinuteOut;

  return validWeather && validSunTimes;
}

// Extract a numeric weather field from a small JSON fragment.
float WiFiManager::extractFloat(const String& src, const String& key) {
  const int index = src.indexOf(key);
  if (index < 0) return NAN;
  const int colon = src.indexOf(':', index);
  if (colon < 0) return NAN;
  const char* start = src.c_str() + colon + 1;
  char* end = nullptr;
  const float value = strtof(start, &end);
  if (end == start || !isfinite(value)) return NAN;
  while (*end && isspace(static_cast<unsigned char>(*end))) ++end;
  if (*end && *end != ',' && *end != '}' && *end != ']') return NAN;
  return value;
}

int WiFiManager::extractIsoTimeMinutes(const String& src, const String& key) {
  int idx = src.indexOf(key);
  if (idx < 0) return -1;

  int timeStart = src.indexOf('T', idx + key.length());
  if (timeStart < 0 || timeStart + 5 >= src.length()) return -1;

  char separator = src[timeStart + 3];
  if (separator != ':') return -1;

  if (!isDigit(src[timeStart + 1]) || !isDigit(src[timeStart + 2])
      || !isDigit(src[timeStart + 4]) || !isDigit(src[timeStart + 5])) return -1;

  int hour = src.substring(timeStart + 1, timeStart + 3).toInt();
  int minute = src.substring(timeStart + 4, timeStart + 6).toInt();
  if (hour > 23 || minute > 59) return -1;

  return hour * 60 + minute;
}

// ===============================
// 🕒 TIME
// ===============================
void WiFiManager::syncWithNTP(uint8_t firstServer) {
  // These esp-prefixed APIs marshal SNTP operations to the TCP/IP thread.
  // Starting a request must not change the timezone before a fresh reply.
  esp_sntp_stop();
  esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
  static const char* const servers[] = {
    "pool.ntp.org", "time.google.com", "time.cloudflare.com"
  };
  for (uint8_t i = 0; i < 3; ++i) {
    esp_sntp_setservername(i, servers[(firstServer + i) % 3]);
  }
  esp_sntp_init();
}

bool WiFiManager::syncNtpNow(uint32_t timeoutMs) {
  if (!isConnected() || !isTimeSyncEnabled() || remainingNetworkTime() == 0) return false;
  ntpSyncReceived.store(false);
  sntp_set_time_sync_notification_cb(onNtpTimeSync);
  syncWithNTP();
  const uint32_t start = millis();
  uint8_t serverIndex = 0;
  const uint32_t slice = timeoutMs / 3;
  while (!ntpSyncReceived.load() && isConnected()) {
    if (millis() - start >= timeoutMs || remainingNetworkTime() == 0) break;
    // Do not depend on lwIP's longer default failover interval: give each
    // server a turn inside this single bounded NTP window.
    if (serverIndex < 2 && millis() - start >= slice * (serverIndex + 1)) {
      syncWithNTP(++serverIndex);
    }
    delay(25);
  }
  // Stop here so late responses cannot silently change a failed attempt.
  esp_sntp_stop();
  return ntpSyncReceived.load();
}

void WiFiManager::refreshTimezoneIfNeeded() {
  if (!autoTimezoneNeedsDetection || remainingNetworkTime() == 0) return;
  int detectedOffsetMinutes;
  if (!detectTimezoneOffsetByIP(detectedOffsetMinutes)) return;
  // Applying TZ is deferred to loop(), after the result queue transfers ownership.
  cachedOffsetMinutes = detectedOffsetMinutes;
  prefs.begin("wifi", false);
  prefs.putInt("offset", cachedOffsetMinutes / 60);
  prefs.putInt("offsetMinutes", cachedOffsetMinutes);
  prefs.end();
  timezoneLoaded = true;
  autoTimezoneNeedsDetection = false;
}

void WiFiManager::applyTimezone() {
  const int minutes = sanitizeTimezoneOffsetMinutes(cachedOffsetMinutes);
  const int magnitude = abs(minutes);
  char zone[24];
  // POSIX TZ signs are reversed: UTC-03:00 means local time is UTC+3.
  snprintf(zone, sizeof(zone), "UTC%s%d:%02d", minutes >= 0 ? "-" : "+",
           magnitude / 60, magnitude % 60);
  setenv("TZ", zone, 1);
  tzset();
}

void WiFiManager::applyManualTimeIfNeeded() {
  if (WiFi.status() == WL_CONNECTED) return;
  if (!manualTimeSet) return;

  // Manual HHMM is wall-clock time. Clear any timezone left by a previous
  // NTP session before mktime() converts it to an epoch.
  static_assert(DEFAULT_MANUAL_TIMEZONE_OFFSET == 0,
                "Manual wall-clock conversion currently requires UTC+0");
  setenv("TZ", "UTC0", 1);
  tzset();

  struct tm t = {};
  t.tm_year = DEFAULT_YEAR - 1900;
  t.tm_mon = 0;
  t.tm_mday = 1;
  t.tm_hour = manualHour;
  t.tm_min = manualMinute;

  struct timeval now = { mktime(&t), 0 };
  settimeofday(&now, nullptr);
}

// ===============================
// 🌍 TIMEZONE
// ===============================
bool WiFiManager::detectTimezoneOffsetByIP(int& offsetMinutesOut) {
  String json;

  // ip-api returns a DST-aware UTC offset in seconds without requiring TLS.
  if (httpGET("http://ip-api.com/json/?fields=status,offset", json) &&
      json.indexOf("\"status\":\"success\"") >= 0) {
    int idx = json.indexOf("\"offset\":");
    if (idx >= 0) {
      int pos = idx + 9;
      while (pos < json.length() && isspace((unsigned char)json[pos])) pos++;
      int sign = 1;
      if (pos < json.length() && json[pos] == '-') { sign = -1; pos++; }
      else if (pos < json.length() && json[pos] == '+') { pos++; }

      long seconds = 0;
      bool hasDigit = false;
      while (pos < json.length() && isDigit(json[pos])) {
        hasDigit = true;
        seconds = seconds * 10L + (json[pos] - '0');
        if (seconds > 14L * 3600L) break;
        pos++;
      }
      seconds *= sign;
      if (hasDigit && seconds >= -14L * 3600L && seconds <= 14L * 3600L &&
          seconds % 60L == 0) {
        offsetMinutesOut = (int)(seconds / 60L);
        return true;
      }
    }
  }

  json = "";
  if (!httpGET("https://ipapi.co/json/", json)) return false;

  int idx = json.indexOf("\"utc_offset\":\"");
  if (idx < 0) return false;

  const int offsetStart = idx + 14;
  if (json.length() < offsetStart + 5) return false;
  String off = json.substring(offsetStart, offsetStart + 5);
  if ((off[0] != '+' && off[0] != '-') || !isDigit(off[1]) ||
      !isDigit(off[2]) || !isDigit(off[3]) || !isDigit(off[4])) return false;

  int sign = (off[0] == '-') ? -1 : 1;
  int hours = off.substring(1, 3).toInt();
  int minutes = off.substring(3, 5).toInt();
  if (hours > 14 || minutes > 59 || (hours == 14 && minutes != 0)) return false;

  offsetMinutesOut = sign * (hours * 60 + minutes);
  return true;
}

// ===============================
// Basic operations
// ===============================
void WiFiManager::stopDisplayTest() {
  if (!displayTestActive) return;
  displayTestActive = false;
  displayTestBrightness = DEFAULT_BRIGHTNESS;
  display.setBrightness(DEFAULT_BRIGHTNESS);
  display.setColonBrightness(DEFAULT_BRIGHTNESS);
  display.setColonBlink(false);
  display.setColon(false);
  display.print("APON");
}

void WiFiManager::handlePortal() {
  server.handleClient();
}

void WiFiManager::stopPortal() {
  dnsServer.stop();
  server.stop();
}

bool WiFiManager::isConfigured() {
  return configured;
}

bool WiFiManager::isConnected() {
  return WiFi.status() == WL_CONNECTED;
}

void WiFiManager::wifiOff() {
  esp_sntp_stop();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

// ===============================
// Cached configuration accessors
// ===============================
bool WiFiManager::isTemperatureEnabled() {
  loadPrefs();
  return showTempCached;
}

bool WiFiManager::isTimeSyncEnabled() {
  loadPrefs();
  return syncTimeCached;
}

int WiFiManager::getBrightness() {
  loadPrefs();
  return brightnessCached;
}

bool WiFiManager::isAutoBrightnessEnabled() {
  loadPrefs();
  return autoBrightnessCached;
}

bool WiFiManager::isBoostModeEnabled() {
  loadPrefs();
  return boostModeCached;
}
