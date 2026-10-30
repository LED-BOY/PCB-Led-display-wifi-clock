# Changelog

Firmware versions and repository maintenance are tracked separately.

## Repository maintenance — 2026-10-30

- Reworked the project homepage with existing photographs, navigation and feature descriptions.
- Added a Spanish project overview and guides for hardware, flashing, setup and validation.
- Moved the unchanged firmware into `Source/GemaLight_ESP_LED_clock_3.6/` so the sketch folder matches its `.ino` filename.
- Documented the local build metadata and the difference between application and merged binaries.
- Added contribution guidance, issue templates, a pull request template and Git housekeeping files.
- Preserved existing hardware files and releases. No new firmware version or hardware validation is claimed.

## 3.6

- Background FreeRTOS worker for Wi-Fi, NTP, timezone and weather.
- Independent time/weather results and retries at 1, 5 and 15 minutes.
- Hourly scheduling based on elapsed time; weather-only retries avoid repeating NTP.
- Two-hour temperature freshness limit and fallback brightness handling.
- Steady colon feedback based on Wi-Fi connection results.
- BOOT-triggered synchronization without queuing duplicate worker jobs.
- Display Test remains active during background captive-portal checks.
- Invalid numeric weather data is rejected.

See the [original 3.6 technical notes](Source/README-3.6.md) for details and limitations.

## 3.4

- Added compatibility with PCB revision v1.7.

## 3.2

- Improved synchronization, weather requests and portal interface.
- Added Boost brightness.

## 2.9

- IP-based automatic setting fixes and minor optimizations.
- Manual mode hides automatic brightness.

## 2.8

- First published release.

Historical summaries are based on the existing [GitHub release notes](https://github.com/LED-BOY/PCB-Led-display-wifi-clock/releases). Release dates are omitted because tag timestamps and publication timestamps do not consistently identify the underlying source build.
