#ifndef DEFAULTVALUES_H
#define DEFAULTVALUES_H
// =======================
// firmware version
// =======================
static constexpr const char* FIRMWARE_VERSION = "3.6";

// =======================
// CPU Frequency
// =======================
constexpr uint32_t CPU_FREQ_MIN = 80;  // MHz used while the clock is idle.
constexpr uint32_t CPU_FREQ_MAX = 120; // MHz used for portal and network operations.

// =======================
// Timing
// =======================
constexpr uint32_t CONFIG_PORTAL_TIME_MS = 60000UL; // 60 s
constexpr uint32_t CONFIG_PORTAL_CONNECTED_TIME_MS = 600000UL; // 10 min
constexpr uint32_t WEATHER_RETRY_INTERVAL_MS = 60000UL;
constexpr uint32_t NTP_TIMEOUT_MS = 10000UL;
constexpr uint32_t SYNC_TOTAL_BUDGET_MS = 45000UL;
constexpr uint32_t AUXILIARY_BUDGET_MS = 15000UL;
constexpr uint32_t HOURLY_SYNC_INTERVAL_MS = 3600000UL;
constexpr uint32_t WEATHER_FRESHNESS_MS = 7200000UL;

// =======================
// Display
// =======================

constexpr uint8_t AUTO_BRIGHTNESS_LOW = 160;
constexpr uint8_t AUTO_BRIGHTNESS_HIGH = 240;
constexpr uint8_t BOOST_BRIGHTNESS_HIGH = 255;
constexpr uint8_t DEFAULT_BRIGHTNESS = AUTO_BRIGHTNESS_HIGH;
constexpr uint8_t TEMPERATURE_BRIGHTNESS_REDUCTION = 30;

// =======================
// Timezone
// =======================

// Until automatic IP detection and an NTP response succeed, use UTC+0.
constexpr int DEFAULT_TIMEZONE_OFFSET = 0;
constexpr int DEFAULT_MANUAL_TIMEZONE_OFFSET = 0;

// =======================
// WiFi
// =======================

// Allow enough time for the first station connection after leaving the setup
// access point, or after a user requests a BOOT-button sync.
constexpr uint32_t WIFI_INTERACTIVE_CONNECT_TIMEOUT = 20000;
// Give scheduled synchronizations the same connection window as BOOT sync.
// Connection waits run in the network task, so the display keeps updating.
constexpr uint32_t WIFI_PERIODIC_CONNECT_TIMEOUT = 20000;

// =======================
// Year
// =======================
constexpr int DEFAULT_YEAR = 2026;

#endif
