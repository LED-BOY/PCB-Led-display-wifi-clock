# Constraint-optimized charlieplexing in firmware 3.8

Firmware `3.8` is integrated into main in the
`Source/GemaLight_ESP_LED_clock_3.8/` sketch folder. It preserves the v1.7
GPIO mapping, portal, time/weather services and OTA interface. It is an
bounded scheduling implementation, not a claim of a new electrical
multiplexing method or an established hardware improvement.

## What runs on the clock

The main task enumerates all 729 HIGH/LOW/Z assignments of GPIO 1, 3, 4,
5, 6 and 7. It rejects assignments that directly select an unwanted LED
or exceed the configured LED-count limits. A greedy exact partition provides
an initial schedule; bounded backtracking searches for fewer groups. Every
segment must occur exactly once. Search exhaustion keeps the verified
incumbent and reports `optimalInDigitalModel: false`.

The search budget bounds recursive search nodes, not wall time or all planner
operations. Candidate enumeration/sorting also takes time. Measure planning
latency on the C3 before raising limits. Plans are rebuilt only when the
segment pattern changes, never inside the timer ISR. The planner workspace
is persistent, not on the FreeRTOS task stack. Its API is main-task-only and
non-reentrant.

An independent verifier checks the chosen schedule. Verification failure
rebuilds a single-LED plan; an invalid fallback publishes a blank frame.
The ISR consumes immutable double-buffered pin masks, releases all previously
driven pins, preloads output latches, enables sinks, then enables sources.
Unused mapped pins have pulls disabled and remain inputs.

## Default build and experimental grouping

**Release default: `COC_ENABLE_GROUPING=1`.** The planner permits up to two LEDs
per group, source and sink, matching the configuration tested by the project
author. Automatic low brightness is 150. Set `COC_ENABLE_GROUPING=0` to use
single-LED scanning with the same exposure and colon-dimming logic.

The published schematic does not show explicit series current-limiting
resistors. For other boards, establish the real current-limiting
arrangement; measure LED, GPIO source/sink and aggregate currents; and check
LED forward-voltage combinations and unintended series paths. HIGH-Z releases
a GPIO driver, not the attached LEDs. Digital verification cannot establish
electrical safety or absence of ghosting. Do not enable grouped mode on the
uncharacterized board merely because a schedule passes the verifier.

On characterized hardware, edit `ConstraintScanConfig.h` or supply build flags:

| Setting | Default | Meaning |
| --- | --- | --- |
| `COC_ENABLE_GROUPING` | 1 | Explicitly permit simultaneous LED selection |
| `COC_MAX_SIMULTANEOUS_LEDS` | 2 | Maximum selected LEDs per group when enabled |
| `COC_MAX_LEDS_PER_SOURCE` | 2 | Maximum LED branches supplied by one GPIO |
| `COC_MAX_LEDS_PER_SINK` | 2 | Maximum LED branches sunk by one GPIO |
| `COC_SEARCH_NODE_LIMIT` | 4096 | Backtracking node budget per changed pattern |

These are branch-count constraints, **not current limits in milliamps**.
There is no current sensing or active current regulation in this firmware.
An electrically safe count must be derived from the actual circuit.

Compile with the settings in [FIRMWARE.md](FIRMWARE.md). For a single-LED
build add `--build-property 'compiler.cpp.extra_flags=-DCOC_ENABLE_GROUPING=0'`
to the documented Arduino CLI compile command. In Arduino IDE, change the
macro in the config header. Existing v3.6 release binaries do not contain this
feature. The author reports qualitative testing of the grouped configuration; this is not electrical certification.

## Brightness and timing

The timer remains at 40 microseconds (25,000 interrupts/second). Each frame
contains 28 exposure quanta, distributed between driven groups and blank
quanta. Every lit segment receives one driven quantum per completed exposure
frame. The inherited gamma lookup and pulse-density modulation gate these
quanta. Scanning advances only on admitted pulses, preventing a brightness
pattern from permanently starving particular segments.

For a gamma duty `g` in 0..64, each segment's ideal ON fraction is
`g / (64 * 28)`, independent of the number of lit segments or groups.
Colon accumulation uses the same denominator without rounding small duty
values to zero. Relative optical colon brightness still needs calibration.

This deliberately changes v3.6 brightness behavior: v3.6 divides exposure
among only the lit segments, so sparse patterns are brighter. This branch
normalizes to the 28-segment case; sparse patterns will be dimmer at the same
setting. At low duty, individual pulses may still be visible or show camera
banding. This is not a flicker-free or higher-resolution dimming claim.

Grouping replaces some driven quanta with blank quanta; it does **not**
reduce the interrupt rate, increase exposure automatically, or establish a
power saving. It may reduce driven GPIO operations. Actual LED current under
simultaneous loading can change, so equal digital ON time alone does not
prove equal brightness. Minimum group count is not an optimized weighted
brightness schedule and does not minimize switching transitions.

## Diagnostics

While the setup portal is running, open
`http://192.168.4.1/display-scan` for JSON containing the firmware version,
grouping status, lit segment count, driven group count, exposure quantum
count, search node count, digital-model optimality and fallback status.
The endpoint is read-only. Use the existing Display Test to exercise `8888`.
The portal is not available after normal setup closes.

## Validation

Run from the repository root:

```sh
tests/run-host-tests.sh
```

The suite compiles the portable planner and the actual display driver against
GPIO/timer stubs. It independently enumerates pin assignments and checks:

- 192 small cases against an exhaustive minimum-partition oracle;
- all 1,440 clock times in single-LED and two-LED modes (2,880 plans);
- 1,000 random patterns/limits, including small search budgets;
- corrupted schedules and zero-budget behavior;
- all 256 brightness inputs in both driver configurations, matching segment
  and colon digital exposure over 1,792 timer calls per input;
- mapped-pin initialization, blank/custom text and pattern changes.

An unrestricted digital-model analysis found 5 groups for `12:34`, 4 for
`11:11`, and 6 for `23:59`. Those are **not** the two-LED configuration and
are not electrical operating recommendations. A bounded search may report a
larger incumbent rather than the optimum; always inspect its status.

GitHub Actions compiles both configurations for the documented ESP32-C3 core
and attaches binary artifacts to successful workflow runs. `grouping-0` is the
alternative single-LED build; `grouping-1` matches the 3.8 release defaults.
Use an application `.ino.bin` for portal OTA; a `.ino.merged.bin` is for initial
USB flashing at address 0x0000. Tagged releases package the grouping-1 firmware, source, checksums and provenance.

Local validation: the host suite passed, and the default firmware compiled
with Arduino-ESP32 3.3.12 for ESP32-C3 using the documented settings (1,196,293
bytes of flash; 44,952 bytes of static RAM). The grouped configuration also compiled successfully (1,196,301 bytes of flash;
44,952 bytes of static RAM). Planner tests passed with AddressSanitizer and
UndefinedBehaviorSanitizer; leak detection was disabled because it is
unavailable in this container. Those earlier local builds were not flashed during that validation session.
Host stubs do not validate ESP interrupt timing, flash/IRAM behavior, current,
ghosting or optical output. Before deployment, run [VALIDATION.md](VALIDATION.md)
and additionally measure the above electrical/timing behavior in both modes,
including Wi-Fi activity, portal tests and OTA flash writes. Compare with
conventional common-anode/common-cathode grouped scans at equal measured
brightness and refresh before claiming an improvement.

## Hardware observations reported for 3.8

The project author tested the grouped feature configuration and reported more
uniform, more natural-looking brightness. At low settings, the colons followed
the digits more accurately instead of disappearing early. No noticeable
increase in maximum brightness was observed. These are qualitative observations
without calibrated optical or current measurements. The fixed exposure budget
and fractional accumulation explain the dimming behavior; the observations do
not isolate a benefit from grouping or prove solver performance.
