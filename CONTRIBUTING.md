# Contributing

## Report a problem

Use the bug report template and include firmware version, PCB revision, ESP32-C3 board variant, power supply, Arduino-ESP32 version and the exact steps to reproduce it. Describe expected and actual results. Photos of the display or soldering can help with hardware issues.

Remove Wi-Fi passwords, public IP addresses and other personal data from logs and screenshots before posting them. For a potential security issue, use GitHub's private vulnerability reporting feature if enabled rather than publishing exploit details in an issue.

## Propose a change

1. Start from the current default branch and keep each change focused.
2. Keep firmware files together in `Source/GemaLight_ESP_LED_clock_3.6/`. Do not commit build outputs, local IDE settings or credentials.
3. Follow the existing code style. Do not change the display GPIO mapping without checking the hardware revision.
4. For firmware changes, compile with the [documented settings](docs/FIRMWARE.md) and run the relevant [device checks](docs/VALIDATION.md).
5. State exactly what was compiled or tested on hardware and what remains untested. For documentation-only changes, check links and factual consistency.
6. Update the documentation and changelog where behavior or installation steps change.

Submit a pull request explaining the problem, resulting behavior and validation. Major hardware changes should include matching schematic, BOM and placement revisions.

The repository has no declared project license. Discuss licensing and permission for third-party material with the maintainer before including it.
