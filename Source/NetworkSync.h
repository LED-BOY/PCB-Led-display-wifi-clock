#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Copied through a queue; the network task never changes display state.
struct NetworkSyncResult {
  bool timeRequested = false;
  bool wifiOk = false;
  bool ntpOk = false;
  bool weatherRequested = false;
  bool weatherOk = false;
  float temperature = -1000;
  int sunriseMinute = -1;
  int sunsetMinute = -1;
  uint32_t elapsedMs = 0;
};

class NetworkSyncController {
public:
  bool start(bool requestTime, bool requestWeather);
  bool poll(NetworkSyncResult& result);

private:
  struct Request {
    bool time;
    bool weather;
    QueueHandle_t results;
  } request{};
  QueueHandle_t results = nullptr;
  bool busy = false;
  static void worker(void* context);
};

extern NetworkSyncController networkSync;
