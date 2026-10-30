# Setup and everyday use

[Home](../README.md) · [Firmware](FIRMWARE.md) · [Hardware](HARDWARE.md)

## Connect to the portal

At power-on or reset, the clock displays `APON` and starts its access point:

| Item | Value |
| --- | --- |
| Network | **ClockSetup** |
| Password | **12345678** |
| Address | [http://192.168.4.1/](http://192.168.4.1/) |
| Initial window | 60 seconds |
| Window after a device joins | 10 minutes from portal startup |

Stay connected even if your phone reports that this network has no internet. Open the address manually if captive-portal detection does not show the page. Reset the clock to reopen the portal after it closes.

<img src="https://github.com/user-attachments/assets/667816ad-a1a0-4d6b-9199-6a6ecc898a90" alt="Clock setup portal on a phone; appearance may vary by firmware version" width="360">

## Settings

| Option | Effect |
| --- | --- |
| Wi-Fi SSID/password | Connects to your router; only 2.4 GHz Wi-Fi is supported |
| Rescan Wi-Fi | Refreshes the network list without discarding current form values |
| Time zone | Auto (IP) detects an offset from your public IP, or select a fixed UTC offset |
| Auto sync time | Uses NTP at startup, then hourly after successful updates |
| Manual time | With automatic sync disabled, enter `HHMM`, for example `0930` |
| Show temperature | Enables outdoor weather display when automatic sync is enabled |
| Auto brightness | In automatic-time mode, uses sunrise/sunset data; falls back to 08:00–20:00 daytime hours |
| Brightness | Manual Low (160) or High (240) on a 0–255 input scale |
| Boost brightness | Raises the selected High level from 240 to 255; Low remains unchanged |
| Save | Stores settings and restarts the clock |
| Cancel | Discards form changes and continues booting with saved settings |
| Display Test | Lights every segment and the colon; temporary brightness is not saved |
| Firmware Update | Uploads a compatible application binary; see the [firmware guide](FIRMWARE.md) |

Automatic brightness is applied only when automatic time synchronization is enabled. Manual-time mode reserves brightness control for the BOOT button. Temperature display reduces the selected brightness by 30.

## BOOT button

| Clock state | BOOT action |
| --- | --- |
| Setup portal | Exits the portal and continues startup |
| Automatic mode, idle | Requests an immediate time/weather update |
| Network work in progress | Consumes the press without queuing another job |
| Manual-time mode | Toggles Low/High brightness |

## Display and synchronization

- `APON`: setup access point is active. `APOF`: portal is closing.
- `SYNC`: initial synchronization is running. Later network jobs keep the regular display updating.
- `----`: no usable time is available.
- **Colon on:** the last network attempt connected to Wi-Fi. NTP or weather may still have failed.
- **Colon off:** manual mode or the last Wi-Fi connection failed.
- During network work, the colon holds its previous steady state.

Time and temperature alternate every 10 seconds when enabled and recent temperature data is available. Successful services refresh hourly. Failed time/weather requests retry independently after 1, 5, then 15 minutes, remaining at 15 minutes until success. Weather-only retries do not repeat NTP. Temperature expires after two hours without a successful refresh.

Wi-Fi waits up to 20 seconds and NTP has a separate 10-second window with backup servers. Network work has a 45-second application budget, but individual DNS/TCP/TLS calls may finish later; this is not a hard real-time deadline.

## Troubleshooting

| Symptom | What to check |
| --- | --- |
| ClockSetup is missing | Reset the board and join during the portal window |
| Phone leaves ClockSetup | Disable automatic switching to mobile data or another network temporarily |
| Router connection fails | Check exact SSID/password, 2.4 GHz availability and signal strength |
| Colon is on but time is wrong | Wi-Fi success does not confirm NTP success; check internet access, NTP and time zone |
| Time zone or weather location is wrong | IP geolocation can follow your ISP or VPN exit; choose a fixed offset for time if needed |
| Temperature disappears | Data expires after two hours; check internet/weather service availability and Show temperature |
| Night brightness is unexpected | Check Auto brightness, automatic time mode and sunrise/sunset availability |
| Segments are missing or swapped | Use Display Test; inspect LED orientation, solder joints, PCB revision and GPIO mapping |
| Upload fails | Check the binary type, COM port and board settings in the [firmware guide](FIRMWARE.md) |

## Timekeeping and network notes

Manual mode is not backed by a battery RTC; verify the time after resets or loss of power. Automatic mode starts with UTC+0 until fresh NTP succeeds during that boot. IP-based timezone detection uses a UTC offset rather than a named timezone rule database; verify the offset after seasonal changes.

Wi-Fi credentials and settings are stored on the device using ESP32 Preferences. NTP, IP location/timezone and weather requests contact external services. The setup network uses a shared default password, the portal uses HTTP, and some remote requests also use HTTP. The HTTPS helper currently disables certificate verification. Use the setup portal in a trusted environment; the current firmware does not implement a hardened provisioning or signed-update system.
