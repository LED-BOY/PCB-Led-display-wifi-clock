# Validation

[Home](../README.md) · [Firmware](FIRMWARE.md)

## Repository update

This update reorganizes the Arduino sketch and improves documentation. Firmware `.ino`, `.cpp` and `.h` files retain their original bytes. PCB, STL, schematic and existing release assets are preserved.

The local `build.options.json` identifies Arduino-ESP32 3.3.12 and the board settings documented in the firmware guide. Existing build files are historical evidence only. No fresh firmware compilation, upload or hardware test was performed for this documentation update.

## Before publishing new firmware

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
