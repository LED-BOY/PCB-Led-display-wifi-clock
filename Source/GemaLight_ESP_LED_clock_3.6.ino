#include "CharlieDisplay.h"
#include "WiFiManager.h"
#include "Utils.h"
#include "NetworkSync.h"
#include "driver/gpio.h"
#include <time.h>

// Runtime configuration and synchronization state
uint8_t brightlevel = DEFAULT_BRIGHTNESS;
bool syncIntervalEnable = false;
bool weatherEnable = false;
bool autoBrightnessEnable = false;
bool boostModeEnable = false;
bool temperatureModeActive = false;
bool syncInProgress = false;
bool ignoreBootUntilReleased = false;


// Cached weather and daylight data
float weatherTemp = -1000;
int sunriseMinute = -1;
int sunsetMinute = -1;
bool sunTimesValid = false;

uint8_t getAutoBrightnessForTime(struct tm* t) {
  int currentMinute = t->tm_hour * 60 + t->tm_min;

  if (syncIntervalEnable && sunTimesValid) {
    return (currentMinute >= sunriseMinute && currentMinute < sunsetMinute)
        ? AUTO_BRIGHTNESS_HIGH
        : AUTO_BRIGHTNESS_LOW;
  }

  return (t->tm_hour >= 20 || t->tm_hour < 8)
      ? AUTO_BRIGHTNESS_LOW
      : AUTO_BRIGHTNESS_HIGH;
}

uint8_t getDisplayBrightness() {
  const uint8_t selectedBrightness =
      (boostModeEnable && brightlevel == AUTO_BRIGHTNESS_HIGH)
          ? BOOST_BRIGHTNESS_HIGH
          : brightlevel;

  if (!temperatureModeActive) return selectedBrightness;

  return (selectedBrightness > TEMPERATURE_BRIGHTNESS_REDUCTION)
      ? selectedBrightness - TEMPERATURE_BRIGHTNESS_REDUCTION
      : 0;
}

void applyDisplayBrightness() {
  const uint8_t level = getDisplayBrightness();
  display.setBrightness(level);
  display.setColonBrightness(level);
}

void applyAutoBrightness(struct tm* t) {
  // Manual-time mode reserves brightness control for the BOOT button.
  if (!syncIntervalEnable || !autoBrightnessEnable || t == nullptr) return;

  uint8_t target = getAutoBrightnessForTime(t);
  if (brightlevel == target) return;

  brightlevel = target;
  applyDisplayBrightness();
}

void updateDisplayMode(bool showTemp, struct tm* t) {

  temperatureModeActive = showTemp && weatherTemp > -999;
  applyDisplayBrightness();

  if (showTemp && weatherTemp > -999) {
    char buf[6];
    utils.formatTemperature(weatherTemp, buf);
    display.print(buf);
  } else {
    display.update(t);
  }
}


// Each service retains its own result and retry schedule.
bool lastWifiConnectionOk = false;
bool lastNtpSyncOk = false;
bool lastWeatherRefreshOk = false;
bool haveNtpSuccess = false;
bool haveWeatherData = false;
uint32_t lastNtpSuccessMs = 0;
uint32_t lastWeatherSuccessMs = 0;
uint32_t lastTimeAttemptMs = 0;
uint32_t lastWeatherAttemptMs = 0;
uint32_t timeRetryDelayMs = 0;
uint32_t weatherRetryDelayMs = 0;
uint8_t timeFailureCount = 0;
uint8_t weatherFailureCount = 0;
bool initialSyncMessage = false;

uint32_t retryDelay(uint8_t failures) {
  if (failures <= 1) return 60000UL;
  if (failures == 2) return 300000UL;
  return 900000UL;
}

bool weatherNeeded() {
  return syncIntervalEnable && (weatherEnable || autoBrightnessEnable);
}

void applyUtcZeroFallback() {
  static_assert(DEFAULT_TIMEZONE_OFFSET == 0,
                "The no-sync reset fallback must remain UTC+0");
  setenv("TZ", "UTC0", 1);
  tzset();
}

void startNetworkSync(bool requestTime, bool requestWeather) {
  if (syncInProgress || !syncIntervalEnable) return;
  setCpuFrequencyMhz(CPU_FREQ_MAX);
  if (!networkSync.start(requestTime, requestWeather)) {
    // Allocation failures also back off, rather than retrying on every loop.
    if (requestTime) {
      lastTimeAttemptMs = millis();
      timeRetryDelayMs = 60000UL;
    }
    if (requestWeather) {
      lastWeatherAttemptMs = millis();
      weatherRetryDelayMs = WEATHER_RETRY_INTERVAL_MS;
    }
    setCpuFrequencyMhz(CPU_FREQ_MIN);
    return;
  }
  syncInProgress = true;
  ignoreBootUntilReleased = true;
  initialSyncMessage = requestTime && !haveNtpSuccess;
  if (initialSyncMessage) display.print("SYNC");
  // Keep the last Wi-Fi result visible until this attempt finishes.
  display.setColonBlink(false);
  display.setColon(syncIntervalEnable && lastWifiConnectionOk);
  // Preserve the established brightness fallback while a refresh is pending.
  if (requestWeather) sunTimesValid = false;
}

void pollNetworkSync() {
  NetworkSyncResult result{};
  if (!networkSync.poll(result)) return;
  const uint32_t now = millis();
  lastWifiConnectionOk = result.wifiOk;
  if (result.timeRequested) {
    lastTimeAttemptMs = now;
    lastNtpSyncOk = result.ntpOk;
    if (result.ntpOk) {
      haveNtpSuccess = true;
      lastNtpSuccessMs = now;
      timeFailureCount = 0;
      timeRetryDelayMs = HOURLY_SYNC_INTERVAL_MS;
      wifi.applyTimezone();
    } else {
      if (timeFailureCount < 3) ++timeFailureCount;
      timeRetryDelayMs = retryDelay(timeFailureCount);
      if (!haveNtpSuccess) applyUtcZeroFallback();
    }
  }
  if (result.weatherRequested) {
    lastWeatherAttemptMs = now;
    lastWeatherRefreshOk = result.weatherOk;
    if (result.weatherOk) {
      if (weatherEnable) weatherTemp = result.temperature;
      sunriseMinute = result.sunriseMinute;
      sunsetMinute = result.sunsetMinute;
      sunTimesValid = true;
      haveWeatherData = true;
      lastWeatherSuccessMs = now;
      weatherFailureCount = 0;
      weatherRetryDelayMs = HOURLY_SYNC_INTERVAL_MS;
    } else {
      sunTimesValid = false;
      if (weatherFailureCount < 3) ++weatherFailureCount;
      weatherRetryDelayMs = retryDelay(weatherFailureCount);
    }
  }
  display.setColonBlink(false);
  // Keep the existing Wi-Fi indicator semantics, even if NTP/weather failed.
  display.setColon(syncIntervalEnable && lastWifiConnectionOk);
  initialSyncMessage = false;
  syncInProgress = false;
  setCpuFrequencyMhz(CPU_FREQ_MIN);
}

// =======================================================
// Setup
// =======================================================
void setup() {
  // Do not apply a saved or detected offset until this boot receives NTP.
  applyUtcZeroFallback();
  utils.setupPins();
  setCpuFrequencyMhz(CPU_FREQ_MAX);

  display.begin();
  display.setBrightness(DEFAULT_BRIGHTNESS);
  // Same 0-255 brightness scale as the charlieplexed display LEDs.
  display.setColonBrightness(DEFAULT_BRIGHTNESS);
  display.print("APON");
  display.setColon(false);

  wifi.startPortalAsync();

  unsigned long t0 = millis();
  uint32_t portalTimeMs = CONFIG_PORTAL_TIME_MS;
  while (millis() - t0 < portalTimeMs) {
    wifi.handlePortal();
    if (WiFi.softAPgetStationNum() > 0) {
      portalTimeMs = CONFIG_PORTAL_CONNECTED_TIME_MS;
    }
    if (wifi.isConfigured() || gpio_get_level((gpio_num_t)BOOT_PIN) == 0) break;
    yield();
  }

  wifi.stopDisplayTest();
  wifi.stopPortal();
  WiFi.softAPdisconnect(true);
  display.print("APOF");
  WiFi.mode(WIFI_STA);
  // When settings have just been saved, allow the radio to finish changing
  // from access-point mode before the first station connection begins.
  delay(500);

  brightlevel = wifi.getBrightness();
  syncIntervalEnable = wifi.isTimeSyncEnabled();
  weatherEnable = syncIntervalEnable && wifi.isTemperatureEnabled();
  autoBrightnessEnable = wifi.isAutoBrightnessEnabled();
  boostModeEnable = wifi.isBoostModeEnabled();

  applyDisplayBrightness();
  display.setColon(false);  // Manual time / portal mode starts with no colon.

  if (!syncIntervalEnable) {
    wifi.wifiOff();
  }

  // Set up a manual clock only in manual mode. Automatic mode keeps UTC+0
  // until a fresh NTP response succeeds during this boot.
  if (!syncIntervalEnable) wifi.applyManualTimeIfNeeded();
  struct tm timeinfo{};
  if (utils.getTimeSafe(timeinfo)) applyAutoBrightness(&timeinfo);

  if (syncIntervalEnable) startNetworkSync(true, weatherNeeded());
  else setCpuFrequencyMhz(CPU_FREQ_MIN);
}

// =======================================================
// LOOP
// =======================================================
void loop() {
  static bool bootWasPressed = false;
  static uint32_t lastBootPress = 0;
  static uint32_t lastRefresh = 0;
  static uint32_t lastToggle = millis();
  static bool showTemp = true;

  pollNetworkSync();
  const uint32_t now = millis();
  const bool bootPressed = gpio_get_level((gpio_num_t)BOOT_PIN) == 0;
  if (!bootPressed) {
    bootWasPressed = false;
    ignoreBootUntilReleased = false;
  } else if (syncInProgress) {
    // A press during network work is consumed and cannot queue another sync.
    ignoreBootUntilReleased = true;
    bootWasPressed = true;
  } else if (!ignoreBootUntilReleased && !bootWasPressed &&
             now - lastBootPress >= 250) {
    bootWasPressed = true;
    lastBootPress = now;
    if (syncIntervalEnable) {
      startNetworkSync(true, weatherNeeded());
    } else {
      brightlevel = brightlevel == AUTO_BRIGHTNESS_LOW
          ? AUTO_BRIGHTNESS_HIGH : AUTO_BRIGHTNESS_LOW;
      applyDisplayBrightness();
    }
  }

  if (now - lastToggle >= 10000UL) {
    lastToggle = now;
    showTemp = !showTemp;
  }
  // Retain a recent temperature through a brief outage, but never display it
  // indefinitely as though it were a current reading.
  if (haveWeatherData && now - lastWeatherSuccessMs >= WEATHER_FRESHNESS_MS) {
    haveWeatherData = false;
    weatherTemp = -1000;
    sunTimesValid = false;
  }

  if (now - lastRefresh >= 250) {
    lastRefresh = now;
    struct tm current{};
    const bool validTime = utils.getTimeSafe(current);
    if (validTime) applyAutoBrightness(&current);
    if (!initialSyncMessage) {
      if (validTime) {
        updateDisplayMode(weatherEnable && haveWeatherData && showTemp, &current);
      } else {
        temperatureModeActive = false;
        applyDisplayBrightness();
        display.print("----");
      }
    }
  }

  if (syncIntervalEnable && !syncInProgress) {
    const bool timeDue = now - lastTimeAttemptMs >= timeRetryDelayMs;
    const bool weatherDue = weatherNeeded() &&
        now - lastWeatherAttemptMs >= weatherRetryDelayMs;
    // A weather-only retry reconnects as needed but never requests NTP.
    if (timeDue || weatherDue) startNetworkSync(timeDue, weatherDue);
  }
  delay(1);  // Yield CPU to the network task and Wi-Fi stack.
}
