# Build, flash and update

[Home](../README.md) · [Hardware](HARDWARE.md) · [Setup](SETUP.md)

> This development branch also contains `3.7-coc-experimental` source.
> See [constraint scanning](CONSTRAINT_SCAN.md) for its opt-in grouped build,
> brightness changes and electrical validation requirements. The v3.6 downloads
> below remain the existing firmware and do not include this experiment.

## Choose the correct binary

Download the files from the [v3.6 release](https://github.com/LED-BOY/PCB-Led-display-wifi-clock/releases/tag/v3.6).

| File | Use |
| --- | --- |
| `GemaLight_ESP_LED_clock_3.6.ino.merged.bin` | Initial USB installation with Espressif Flash Download Tool at address `0x0000` |
| `GemaLight_ESP_LED_clock_3.6.ino.bin` | Application update through the clock's Firmware Update portal |

The merged image includes bootloader/partition data. Select the application image when updating from the portal. Existing release binaries are retained; this repository documentation update does not rebuild them or establish that they match the current source commit.

## Arduino IDE

1. Install Arduino IDE and add the Espressif board package index in Preferences if it is not already configured:

   ```text
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```

2. Install **esp32 by Espressif Systems** from Boards Manager. The code uses Arduino-ESP32 **3.x** timer APIs; 2.x is incompatible. The supplied local build metadata records core **3.3.12**. That is a reference environment, not a fresh compilation result.
3. Download or clone this repository. Open `Source/GemaLight_ESP_LED_clock_3.6/GemaLight_ESP_LED_clock_3.6.ino`.
4. Keep every `.cpp` and `.h` file beside the `.ino`. The sketch uses libraries included with Arduino-ESP32; do not install a separate third-party WiFiManager library for this project.
5. Select **ESP32C3 Dev Module**, your serial port and the settings below. Verify, then upload.

| Setting | Value recorded in the supplied build metadata |
| --- | --- |
| Board | ESP32C3 Dev Module (`esp32:esp32:esp32c3`) |
| Upload speed | 921600; lower it if uploads are unreliable |
| USB CDC on boot | Default / disabled (`CDCOnBoot=default`) |
| CPU frequency | 160 MHz build setting; firmware switches to 80/120 MHz at runtime |
| Flash frequency | 80 MHz |
| Flash mode | QIO |
| Flash size | 4 MB |
| Partition scheme | Minimal SPIFFS (`min_spiffs`, with OTA application slots) |
| Core debug level | None |
| Erase all flash before upload | Disabled |

These settings must match the physical board. The project includes an earlier Arduino settings screenshot for comparison:

<img src="https://github.com/user-attachments/assets/72462af9-1677-4572-b6ab-506296b2d445" alt="Arduino IDE board settings from the original project" width="616">

## Arduino CLI

Run from the repository root with Arduino CLI installed:

```sh
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@3.3.12 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli compile --fqbn "esp32:esp32:esp32c3:UploadSpeed=921600,CDCOnBoot=default,CPUFreq=160,FlashFreq=80,FlashMode=qio,FlashSize=4M,PartitionScheme=min_spiffs,DebugLevel=none" --export-binaries Source/GemaLight_ESP_LED_clock_3.6
```

Reference: [Arduino CLI getting started](https://docs.arduino.cc/arduino-cli/getting-started/), [sketch folder specification](https://docs.arduino.cc/arduino-cli/sketch-specification/) and [ESP32 3.0 migration guide](https://docs.espressif.com/projects/arduino-esp32/en/latest/migration_guides/2.x_to_3.0.html).

## Initial installation with Espressif Flash Download Tool

1. Download the tool from [Espressif's official documentation](https://docs.espressif.com/projects/esp-test-tools/en/latest/esp32/production_stage/tools/flash_download_tool.html).
2. Select the **ESP32-C3** chip and **UART** download mode.
3. Select the `*.ino.merged.bin` file, enable its row and enter address **`0x0000`**.
4. Select the correct COM port and start flashing.
5. Wait for completion, then reset the board manually.

<img src="https://github.com/user-attachments/assets/1335b5ff-ac59-4f1b-9661-26ab083455e4" alt="Espressif Flash Download Tool with merged image at address zero" width="523">

If the board does not enter download mode automatically, hold BOOT while resetting it, then release BOOT and retry. Normal Arduino uploads handle the bootloader and partition data; a separate Burn Bootloader step is not normally required.

## Update through the portal

1. Reset the clock and join **ClockSetup** with password **12345678**.
2. Open [192.168.4.1](http://192.168.4.1/) and choose **Firmware Update**.
3. Select the compatible **`*.ino.bin` application file**.
4. Keep power connected until the update completes and the clock restarts.
5. Recheck settings and run the [device validation checklist](VALIDATION.md).
