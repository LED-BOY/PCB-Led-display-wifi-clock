#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <deafultValues.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <Preferences.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <time.h>
#include <HTTPClient.h>
#include <Update.h>

class WiFiManager {

public:
  WiFiManager();

  // Wi-Fi connection
  bool tryReconnect(uint32_t timeoutMs = WIFI_INTERACTIVE_CONNECT_TIMEOUT);

  // Configuration portal
  void startPortalAsync();
  void handlePortal();
  void stopPortal();
  void stopDisplayTest();

  // Connection state
  bool isConnected();
  bool isConfigured();

  // Time
  void applyManualTimeIfNeeded();
  bool syncNtpNow(uint32_t timeoutMs = NTP_TIMEOUT_MS);
  // Called only by the network worker, except applyTimezone() in loop().
  void setNetworkBudget(uint32_t durationMs);
  uint32_t remainingNetworkTime() const;
  void refreshTimezoneIfNeeded();
  void applyTimezone();

  // Weather
  bool getWeatherAuto(float &tempOut,
                      int &sunriseMinuteOut, int &sunsetMinuteOut);

  // Cached configuration
  bool isTimeSyncEnabled();
  bool isTemperatureEnabled();
  bool isAutoBrightnessEnabled();
  bool isBoostModeEnabled();
  int getBrightness();

  // Power management
  void wifiOff();

private:
  Preferences prefs;
  WebServer server{80};
  DNSServer dnsServer;

  bool configured = false;
  uint32_t networkBudgetStart = 0;
  uint32_t networkBudgetMs = SYNC_TOTAL_BUDGET_MS;
  bool displayTestActive = false;
  uint8_t displayTestBrightness = DEFAULT_BRIGHTNESS;

  // =========================
  // OTA update state
  // =========================
  bool otaUpdateOk = false;

  // =========================
  // Preferences cache
  // =========================
  bool syncTimeCached = true;
  bool showTempCached = false;
  bool autoBrightnessCached = false;
  bool boostModeCached = false;
  int brightnessCached = DEFAULT_BRIGHTNESS;

  bool prefsLoaded = false;
  bool weatherLocationCached = false;
  float weatherLatitude = 0.0f;
  float weatherLongitude = 0.0f;

  void loadPrefs();

  // =========================
  // 🕒 Manual time
  // =========================
  bool manualTimeSet = false;
  int manualHour = -1;
  int manualMinute = -1;

  // =========================
  // 🌍 Timezone cache
  // =========================
  int cachedOffsetMinutes = DEFAULT_TIMEZONE_OFFSET * 60;
  bool timezoneLoaded = false;
  bool autoTimezoneNeedsDetection = false;

  // =========================
  // 🔧 Helpers
  // =========================
  bool connect(uint32_t timeoutMs);
  bool httpGET(const String& url, String& response);
  float extractFloat(const String& src, const String& key);
  int extractIsoTimeMinutes(const String& src, const String& key);

  // =========================
  // 🕒 Time
  // =========================
  void syncWithNTP(uint8_t firstServer = 0);
  bool detectTimezoneOffsetByIP(int& offsetMinutesOut);
};

extern WiFiManager wifi;

#endif
