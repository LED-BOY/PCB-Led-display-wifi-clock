# Validation

[Home](../README.md) · [Firmware](FIRMWARE.md)

## Firmware 3.8 validation status

The release workflow runs the existing portable planner and display-driver tests, then compiles both grouping configurations with Arduino-ESP32 3.3.12. Tagged release packaging depends on all three jobs passing. Release firmware uses grouping enabled and automatic low brightness 150.

The project author reports testing the grouped feature configuration on the clock: more uniform, natural-looking brightness and closer colon tracking at low settings, with no noticeable maximum-brightness increase. These qualitative observations do not establish measured current limits, optical calibration, interrupt timing, or a complete Wi-Fi/OTA regression run for the final 3.8 build.

The checklist below records further device checks; items are not marked complete without evidence. Historical 3.6 build data is not validation of 3.8.

## Device validation checklist

1. Compile using the documented core, board and OTA-capable partition scheme. Record tool versions, source commit and flash/RAM usage.
2. Flash a compatible board and confirm the portal opens at startup, saves settings and closes correctly.
3. With a working router, verify initial NTP, later background synchronization, correct timezone and continued display refresh.
4. Turn off the router. Confirm the clock continues running, the colon turns off after failure and retries follow 1/5/15 minutes.
5. Block NTP while keeping Wi-Fi available. Verify backup-server attempts, preservation of an already-running clock and a lit colon after Wi-Fi success.
6. Make weather unavailable. Check weather-only retries, eventual temperature expiry and fallback brightness.
7. Press BOOT while idle and during network work. Confirm immediate idle synchronization and no queued duplicate jobs.
8. In manual mode, check manual time and BOOT Low/High brightness switching.
9. Run Display Test while the phone performs captive-portal checks. Confirm it remains active until explicitly stopped or the portal exits.
10. Test a portal update with an application binary and restart the device. Publish application and merged images with clear filenames, hashes and source provenance.
