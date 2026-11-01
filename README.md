# GemaLight · PCB LED Wi-Fi Clock

> **Experimental branch:** includes `3.7-coc-experimental` constraint-optimized
> scanning. Grouped driving is OFF by default pending electrical validation.
> Read [the experiment, build settings and validation](docs/CONSTRAINT_SCAN.md).
> Existing v3.6 release binaries do not include these changes.

A four-digit clock that turns translucent FR4 into an LED display. An ESP32-C3 drives charlieplexed LEDs to show the time and outdoor temperature, with a browser-based setup portal and wireless firmware updates.

[![Firmware](https://img.shields.io/badge/firmware-3.6-2563eb)](https://github.com/LED-BOY/PCB-Led-display-wifi-clock/releases/tag/v3.6)
[![Platform](https://img.shields.io/badge/platform-ESP32--C3-333333)](docs/FIRMWARE.md)
[![Arduino](https://img.shields.io/badge/Arduino--ESP32-3.x-008184)](docs/FIRMWARE.md)

**English** · [Español](README.es.md)

[Get firmware](https://github.com/LED-BOY/PCB-Led-display-wifi-clock/releases/tag/v3.6) · [Build the clock](docs/HARDWARE.md) · [Flash firmware](docs/FIRMWARE.md) · [Configure](docs/SETUP.md)

<p align="center">
  <img src="https://github.com/user-attachments/assets/0c8ef493-8c9e-4351-bac5-b09b48d5e743" alt="GemaLight clock assembled with its translucent PCB display" width="900">
</p>

## What it does

- **Time over Wi-Fi:** NTP synchronization at startup and hourly after success, with backup servers and automatic retries.
- **Outdoor temperature:** optional weather data from Open-Meteo using an approximate location derived from the public IP address; no onboard temperature sensor is used.
- **Automatic brightness:** sunrise/sunset scheduling, a night fallback, manual Low/High levels and optional Boost.
- **Offline operation:** manual time configuration without an internet connection.
- **Phone-friendly setup:** configure Wi-Fi, time, brightness and display options from a captive portal.
- **Wireless updates and diagnostics:** upload an application binary and test all segments from the setup portal.
- **Background network work:** version 3.6 keeps the display refreshing while synchronization runs.

## Start here

1. Assemble a compatible ESP32-C3 Super Mini and PCB. Check the [hardware files and revision notes](docs/HARDWARE.md).
2. [Flash the firmware](docs/FIRMWARE.md). Use a merged binary for first installation or an application binary for a portal update.
3. Power on or reset the clock. Join **ClockSetup**, password **12345678**.
4. Open [192.168.4.1](http://192.168.4.1/) if the setup page does not appear automatically.
5. Enter a **2.4 GHz** Wi-Fi network, choose your settings and press **Save**. The clock restarts.

The portal opens for 60 seconds at boot, extending to a 10-minute window when a device joins. Reset the clock to reopen it later. See the [setup guide](docs/SETUP.md) for button actions, indicators and troubleshooting.

## Design

LEDs mounted sideways illuminate the FR4 board. Charlieplexing selects each segment using pairs of GPIOs; a separate output drives the colon. A printed diffuser helps define the digits and reduce light bleed. Camera exposure can make the multiplexed display look less distinct than it does in person.

<table>
  <tr>
    <td><img src="https://github.com/user-attachments/assets/c8df0108-78f4-4db1-98c9-d2e753f6749f" alt="Clock showing the time" width="400"></td>
    <td><img src="https://github.com/user-attachments/assets/840e3d11-ebf7-4f96-81f5-093efb4d8e2f" alt="Clock showing outdoor temperature" width="400"></td>
  </tr>
  <tr><td align="center">Time display</td><td align="center">Outdoor temperature</td></tr>
</table>

## Repository guide

| Location | Contents |
| --- | --- |
| [Source](Source/) | Arduino sketch and version 3.6 technical notes |
| [PCB assembly](PCB%20assembly/) | Gerbers, BOMs, placement data and LED orientation |
| [STL files](STL%20files/) | Case, diffuser and stand models |
| [Schematic v1.7](schematic%20v1.7.png) | Circuit reference |
| [Hardware guide](docs/HARDWARE.md) | Parts, revisions and assembly |
| [Firmware guide](docs/FIRMWARE.md) | Arduino settings, USB flashing and portal updates |
| [Setup guide](docs/SETUP.md) | Everyday use and troubleshooting |
| [Changelog](CHANGELOG.md) | Firmware history and repository improvements |

## Firmware 3.6

A single FreeRTOS worker handles Wi-Fi, NTP, timezone and weather requests. Time and weather track their results independently and retry after **1, 5 and 15 minutes** on failure. Recent temperature data expires after two hours. The colon reports the last Wi-Fi connection result; it does not confirm that NTP or weather succeeded.

See [version 3.6 technical notes](Source/README-3.6.md) and the [device validation checklist](docs/VALIDATION.md). The source layout and documentation update does not change firmware behavior or replace the existing release binaries.

## Contributing

Bug reports, build notes and improvements are welcome. Read [CONTRIBUTING.md](CONTRIBUTING.md) and include your PCB revision, firmware version and Arduino-ESP32 version when reporting a problem.

## Credits and licensing

Created by [LED-BOY](https://github.com/LED-BOY). Built with [Arduino-ESP32](https://github.com/espressif/arduino-esp32); weather data is provided by [Open-Meteo](https://open-meteo.com/), with IP-based location/timezone services from [ip-api](https://ip-api.com/) and [ipapi](https://ipapi.co/).

No project license has been declared in this repository. Contact the author about permission to reuse or redistribute the code, PCB files, models or photographs. Third-party projects and services retain their own terms.
