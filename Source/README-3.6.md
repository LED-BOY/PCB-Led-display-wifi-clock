# GemaLight ESP LED Clock 3.6

Open `GemaLight_ESP_LED_clock_3.6.ino` from this folder in Arduino IDE.
Use the same ESP32-C3 board settings and installed libraries as version 3.5.
Keep `NetworkSync.cpp` and `NetworkSync.h` alongside the sketch.

## Changes

- A single FreeRTOS network worker performs connection, NTP, timezone and
  weather requests. The main loop continues refreshing the display and handling
  brightness. Initial synchronization shows SYNC; later synchronizations keep
  the time/temperature display updating.
- Wi-Fi connection has a 20-second limit. NTP has one 10-second window,
  rotating pool.ntp.org, time.google.com and time.cloudflare.com. A fresh NTP
  callback ends the NTP wait immediately; an already-valid clock is not accepted
  as proof of a new synchronization.
- Optional timezone/weather work shares up to 15 seconds of remaining work
  budget. The overall application budget is 45 seconds. Requests are not
  started after that budget expires; HTTP bodies are read with deadline and
  size checks. Individual Arduino DNS, TCP, HTTP-header and TLS calls can finish
  after the application deadline; this is not a hard real-time 45-second cutoff.
  The display keeps updating while those calls finish.
- Failed time updates retry after 1, 5, then 15 minutes, staying at 15 minutes
  until success. A successful update schedules the next one an hour later,
  using elapsed time so timezone changes cannot trigger duplicate hourly jobs.
- Weather has its own retry schedule and success status. A weather-only retry
  connects Wi-Fi but does not restart NTP. Recent temperatures remain available
  during a short outage and expire after two hours. Failed daylight refreshes
  retain the existing 08:00–20:00 brightness fallback.
- Wi-Fi, NTP and weather results are stored separately. The colon retains its
  Wi-Fi feedback without blinking: steady at the previous result during network
  work, then on if Wi-Fi connected,
  even if NTP or weather failed. Manual-time mode keeps it off.
- BOOT requests an immediate update while idle. Presses during network work
  are consumed rather than queued. Only one worker can run at a time.
- SNTP and Wi-Fi are shut down before a worker reports completion. Timezone
  changes are applied by the main loop after receiving the result. A failed
  sync does not reset an already-running clock or its timezone.
- The Display Test fix from 3.5 is included: background requests to the setup
  page cannot stop the test and restore AP ON.
- Invalid numeric weather fields are rejected instead of being read as zero.

## Validation

Source reviewed and checked for file consistency and balanced delimiters.
Not compiled, uploaded, or tested on hardware, as requested.

Suggested device checks after uploading:

1. With a working router, verify that synchronization finishes early, the colon
   becomes steady, and the display keeps changing during subsequent syncs.
2. Turn off the router: verify that connection waits end, the clock continues,
   the colon turns off, and retries follow the 1/5/15-minute schedule.
3. Block NTP while leaving Wi-Fi available: verify backup-server attempts,
   NTP failure status and preservation of the clock; the colon should remain on.
4. Make weather unavailable: verify weather-only retries and brightness fallback.
5. Open Display Test and wait while the phone performs captive-portal checks;
   it should stay active until an explicit stop or portal exit.
