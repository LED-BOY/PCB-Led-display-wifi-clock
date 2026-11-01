#include "NetworkSync.h"
#include "WiFiManager.h"
#include <freertos/task.h>

NetworkSyncController networkSync;

bool NetworkSyncController::start(bool requestTime, bool requestWeather) {
  if (busy) return false;
  if (!results) results = xQueueCreate(1, sizeof(NetworkSyncResult));
  if (!results) return false;
  request = {requestTime, requestWeather, results};
  // A single worker owns WiFiManager until its result has been consumed.
  if (xTaskCreate(worker, "clock-network", 12288, &request, 1, nullptr) != pdPASS) {
    return false;
  }
  busy = true;
  return true;
}

bool NetworkSyncController::poll(NetworkSyncResult& result) {
  if (!busy || xQueueReceive(results, &result, 0) != pdTRUE) return false;
  busy = false;
  return true;
}

void NetworkSyncController::worker(void* context) {
  const Request job = *static_cast<Request*>(context);
  NetworkSyncResult result{};
  result.timeRequested = job.time;
  result.weatherRequested = job.weather;
  const uint32_t started = millis();
  wifi.setNetworkBudget(SYNC_TOTAL_BUDGET_MS);
  result.wifiOk = wifi.tryReconnect(WIFI_INTERACTIVE_CONNECT_TIMEOUT);

  if (result.wifiOk && job.time) {
    // One NTP session with three servers, ending on the first fresh response.
    result.ntpOk = wifi.syncNtpNow(NTP_TIMEOUT_MS);
  }

  // Optional services share the remaining auxiliary budget. They cannot
  // restart NTP, extend the total budget, or turn a successful NTP into failure.
  const uint32_t remaining = wifi.remainingNetworkTime();
  wifi.setNetworkBudget(remaining < AUXILIARY_BUDGET_MS
      ? remaining : AUXILIARY_BUDGET_MS);
  if (result.ntpOk) wifi.refreshTimezoneIfNeeded();
  if (result.wifiOk && job.weather && wifi.remainingNetworkTime() > 0) {
    result.weatherOk = wifi.getWeatherAuto(
        result.temperature, result.sunriseMinute, result.sunsetMinute);
  }

  // Always stop SNTP and the radio, including timeout/failure paths.
  wifi.wifiOff();
  result.elapsedMs = millis() - started;
  xQueueSend(job.results, &result, portMAX_DELAY);
  vTaskDelete(nullptr);
}
