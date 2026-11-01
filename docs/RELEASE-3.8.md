Firmware 3.8 integrates constraint-optimized charlieplexing into `main`.

- Bounded GPIO schedule planning, independent verification and single-LED fallback.
- Fixed 28-quantum per-segment exposure and fractional colon pulse accumulation for consistent dimming across patterns and better low-level colon tracking.
- The project author reports more uniform, natural-looking brightness on the grouped feature build, with no noticeable increase in maximum brightness. These are qualitative hardware observations; grouping alone is not established as their cause.
- Preserves the tested defaults: **grouping enabled**, up to **two LEDs per group/source/sink**, and **automatic low brightness 150**.
- Read-only `/display-scan` diagnostics in the setup portal.
- Updated source layout, English/Spanish READMEs and constraint scan explanation.

Download `GemaLight-3.8-source.zip` for the full tagged source. The Arduino sketch is `Source/GemaLight_ESP_LED_clock_3.8/GemaLight_ESP_LED_clock_3.8.ino`.

For portal OTA, use `GemaLight_ESP_LED_clock_3.8.ino.bin`. For initial USB flashing at `0x0000`, use `GemaLight_ESP_LED_clock_3.8.ino.merged.bin`. Other binary files are build components, not portal updates. Check `SHA256SUMS.txt` and `BUILD-INFO.txt` for integrity and provenance.

Release publication requires the host planner/driver suite and ESP32-C3 builds with grouping both disabled and enabled to pass. Release binaries use Arduino-ESP32 3.3.12 and grouping enabled. The final build has not received a complete device Wi-Fi/OTA regression test in this release session. LED-count limits are not current regulation; electrical current, optical calibration and interrupt timing require measurement on the actual board. Low-duty pulses can remain visible, and sparse patterns may be dimmer than in 3.6 at the same setting.

See [constraint scanning](https://github.com/LED-BOY/PCB-Led-display-wifi-clock/blob/v3.8/docs/CONSTRAINT_SCAN.md), [build instructions](https://github.com/LED-BOY/PCB-Led-display-wifi-clock/blob/v3.8/docs/FIRMWARE.md) and [validation](https://github.com/LED-BOY/PCB-Led-display-wifi-clock/blob/v3.8/docs/VALIDATION.md).
