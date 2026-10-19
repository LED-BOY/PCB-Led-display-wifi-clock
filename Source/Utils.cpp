#include "Utils.h"

#include <math.h>

Utils utils;

void Utils::setupPins() {
  pinMode(BOOT_PIN, INPUT_PULLUP);
}

// =======================================================
// Safe system-time access
// =======================================================
bool Utils::getTimeSafe(struct tm &t) {
  const time_t now = time(nullptr);
  if (now < 100000) return false;

  struct tm candidate;
  if (localtime_r(&now, &candidate) == nullptr) return false;

  t = candidate;
  return true;
}

// =======================================================
// Temperature formatting
// =======================================================
void Utils::formatTemperature(float temp, char *out) {

  if (out == nullptr) return;

  if (!isfinite(temp) || temp <= -999) {
    out[0] = '\0';
    return;
  }

  if (temp > 99.0f) temp = 99.0f;
  if (temp < -99.0f) temp = -99.0f;

  // Round only values above the half-degree mark up. This intentionally keeps
  // xx.5 at the lower whole degree: 10.4 -> 10, 10.5 -> 10, 10.7 -> 11.
  const bool negative = temp < 0.0f;
  const float magnitude = negative ? -temp : temp;
  int tempInt = static_cast<int>(magnitude);
  if (magnitude - tempInt > 0.5f) tempInt++;

  int pos = 0;

  if (negative) {
    out[pos++] = '-';
  }

  bool twoDigits = (tempInt >= 10);

  if (twoDigits) {
    out[pos++] = '0' + (tempInt / 10);
  }

  out[pos++] = '0' + (tempInt % 10);
  out[pos++] = (char)36;  // Custom degree-glyph index used by CharlieDisplay.

  out[pos] = '\0';
}
