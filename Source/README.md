# Firmware source

Open [GemaLight_ESP_LED_clock_3.6/GemaLight_ESP_LED_clock_3.6.ino](GemaLight_ESP_LED_clock_3.6/GemaLight_ESP_LED_clock_3.6.ino) in Arduino IDE. Keep all `.cpp` and `.h` files in that same sketch folder.

The sketch directory now matches the main `.ino` filename, as required by Arduino. Firmware files are preserved without code changes. `deafultValues.h` retains its original spelling to keep existing includes valid.

| File | Responsibility |
| --- | --- |
| `GemaLight_ESP_LED_clock_3.6.ino` | Startup, display modes, brightness and retry scheduling |
| `CharlieDisplay.cpp/.h` | Charlieplexed segments, colon, font and timer interrupt |
| `WiFiManager.cpp/.h` | Setup portal, settings, network requests and OTA |
| `NetworkSync.cpp/.h` | FreeRTOS network worker and result queue |
| `Utils.cpp/.h` | Pin setup, time access and temperature formatting |
| `deafultValues.h` | Firmware version, timing and brightness defaults |

See [build and flash instructions](../docs/FIRMWARE.md), [3.6 notes](README-3.6.md) and [validation](../docs/VALIDATION.md).
