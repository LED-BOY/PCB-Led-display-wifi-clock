#pragma once

#include <Arduino.h>
#include <driver/gpio.h>
#include "deafultValues.h"
#include "ConstraintScan.h"
#include "ConstraintScanConfig.h"

// =========================
// CONFIG
// =========================
#define DIGITS 4
#define SEGMENTS 7
#define MAX_ACTIVE 28

// Colon LEDs have their anodes on GPIO 0 and cathodes tied to GND.
#define COLON_PIN 0

class CharlieDisplay {

public:
  CharlieDisplay();

  void begin();
  void update(struct tm* t);
  void print(const char* str);

  void setColon(bool on);
  void setColonBlink(bool enable);
  void setColonBrightness(uint8_t level);

  // Sets gamma-corrected brightness using pulse-density modulation.
  void setBrightness(uint8_t level);

  friend void IRAM_ATTR onTimer();

  struct ScanStats {
    uint8_t lit, groups;
    uint32_t searchNodes;
    bool optimal, fallback, groupingEnabled;
  };
  ScanStats getScanStats();

private:

  // =========================
  // Seven-segment font
  // =========================
  const uint8_t font[39][7] = {
    // 0–9
    { 1, 1, 1, 1, 1, 1, 0 },
    { 0, 1, 1, 0, 0, 0, 0 },
    { 1, 1, 0, 1, 1, 0, 1 },
    { 1, 1, 1, 1, 0, 0, 1 },
    { 0, 1, 1, 0, 0, 1, 1 },
    { 1, 0, 1, 1, 0, 1, 1 },
    { 0, 0, 1, 1, 1, 1, 1 },
    { 1, 1, 1, 0, 0, 0, 0 },
    { 1, 1, 1, 1, 1, 1, 1 },
    { 1, 1, 1, 0, 0, 1, 1 },

    // A–Z
    { 1, 1, 1, 0, 1, 1, 1 },
    { 0, 0, 1, 1, 1, 1, 1 },
    { 1, 0, 0, 1, 1, 1, 0 },
    { 0, 1, 1, 1, 1, 0, 1 },
    { 1, 0, 0, 1, 1, 1, 1 },
    { 1, 0, 0, 0, 1, 1, 1 },
    { 1, 0, 1, 1, 1, 1, 0 },
    { 0, 1, 1, 0, 1, 1, 1 },
    { 0, 1, 1, 0, 0, 0, 0 },
    { 0, 1, 1, 1, 0, 0, 0 },
    { 0, 0, 0, 0, 1, 1, 1 },
    { 0, 0, 0, 1, 1, 1, 0 },
    { 1, 1, 1, 0, 1, 1, 0 },
    { 0, 0, 1, 0, 1, 0, 1 },
    { 0, 0, 1, 1, 1, 0, 1 },
    { 1, 1, 0, 0, 1, 1, 1 },
    { 1, 1, 1, 0, 0, 1, 1 },
    { 0, 0, 0, 0, 1, 0, 1 },
    { 1, 0, 1, 1, 0, 1, 1 },
    { 0, 0, 0, 1, 1, 1, 1 },
    { 0, 1, 1, 1, 1, 1, 0 },
    { 0, 1, 1, 1, 1, 1, 0 },
    { 0, 1, 1, 1, 1, 1, 0 },
    { 0, 1, 1, 0, 1, 1, 1 },
    { 0, 1, 1, 1, 0, 1, 1 },
    { 1, 1, 0, 1, 1, 0, 1 },

    // 36: custom degree glyph
    { 1, 1, 0, 0, 0, 1, 1 },

    // 37 → '-'
    { 0, 0, 0, 0, 0, 0, 1 },

    // 38: blank
    { 0, 0, 0, 0, 0, 0, 0 }
  };

  // =========================
  // Display state
  // =========================
  volatile uint8_t charBuffer[DIGITS] = { 0 };

  // Double-buffered immutable ISR plans. Only the main task builds plans.
  coc::Planner planner;
  coc::Slot scanSlots[2][MAX_ACTIVE]{};
  volatile uint8_t publishedPlan = 0;
  uint32_t publishedTarget = 0xFFFFFFFFu;
  ScanStats scanStats{};
  volatile uint8_t activeCount = 0;
  void publishCharacters(const uint8_t chars[DIGITS]);

  // =========================
  // Brightness state (pulse-density modulation)
  // =========================
  volatile uint8_t brightness;
  volatile uint8_t effectiveDuty;
  volatile uint8_t frame = 0;  // PWM accumulator, 0-63

  // =========================
  // COLON
  // =========================
  volatile bool colonEnabled = true;
  volatile bool colonBlink = false;
  volatile uint16_t colonBlinkTicks = 0;
  // Same 0-255, gamma-corrected scale as setBrightness().
  volatile uint8_t colonBrightnessLevel;
  // Gamma increment (0-64); accumulator denominator is 64*28.
  volatile uint8_t colonBrightness;
  volatile uint16_t colonPwmCounter;  // accumulator, 0-(64*28-1)

  // =========================
  // ISR
  // =========================
  volatile uint8_t isrStep = 0;
  volatile uint8_t drivenPins = 0;

  // =========================
  // TIMER
  // =========================
  hw_timer_t* timer = nullptr;
  portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

  uint8_t charToIndex(char c);
};

extern CharlieDisplay display;
