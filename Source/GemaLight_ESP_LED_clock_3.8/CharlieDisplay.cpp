#include "CharlieDisplay.h"

CharlieDisplay display;

static const uint8_t gammaTable[256] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5,
  5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7,
  7, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 10, 10, 10, 10,
  10, 11, 11, 11, 11, 12, 12, 12, 12, 12, 13, 13, 13, 13, 14, 14,
  14, 14, 15, 15, 15, 15, 16, 16, 16, 16, 17, 17, 17, 17, 18, 18,
  18, 18, 19, 19, 19, 20, 20, 20, 21, 21, 21, 21, 22, 22, 22, 23,
  23, 23, 24, 24, 24, 25, 25, 25, 26, 26, 26, 27, 27, 27, 28, 28,
  28, 29, 29, 29, 30, 30, 30, 31, 31, 32, 32, 32, 33, 33, 34, 34,
  34, 35, 35, 35, 36, 36, 37, 37, 38, 38, 38, 39, 39, 40, 40, 40,
  41, 41, 42, 42, 43, 43, 44, 44, 44, 45, 45, 46, 46, 47, 47, 48,
  48, 49, 49, 50, 50, 51, 51, 51, 52, 52, 53, 53, 54, 54, 55, 55,
  56, 57, 57, 58, 58, 59, 59, 60, 60, 61, 61, 62, 62, 63, 63, 64
};


// Fixed 28-quanta exposure budget: each lit segment receives one quantum.
// Idle quanta retain the same per-segment duty when grouping reduces slots.
static constexpr uint16_t kColonDenominator = 64 * MAX_ACTIVE;
// Keep the ISR lookup in RAM rather than flash-backed constant storage.
static DRAM_ATTR uint8_t scanGpio[coc::kPins] = {
  coc::kGpio[0], coc::kGpio[1], coc::kGpio[2],
  coc::kGpio[3], coc::kGpio[4], coc::kGpio[5]
};

static inline void IRAM_ATTR releasePins(uint8_t mask) {
  for (uint8_t p = 0; p < coc::kPins; ++p)
    if (mask & (1u << p))
      gpio_set_direction((gpio_num_t)scanGpio[p], GPIO_MODE_INPUT);
}

void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&display.timerMux);
  const uint16_t colonSum = display.colonPwmCounter + display.colonBrightness;
  const bool colonPulse = colonSum >= kColonDenominator;
  display.colonPwmCounter = colonPulse ? colonSum - kColonDenominator : colonSum;
  if (display.colonBlink && ++display.colonBlinkTicks >= 25000)
    display.colonBlinkTicks = 0;
  const bool colonVisible = !display.colonBlink || display.colonBlinkTicks < 12500;
  gpio_set_level((gpio_num_t)COLON_PIN,
                 display.colonEnabled && colonVisible && colonPulse);

  // Break before make: all formerly driven outputs become high impedance.
  releasePins(display.drivenPins);
  display.drivenPins = 0;
  const uint8_t sum = display.frame + display.effectiveDuty;
  const bool pulse = sum >= 64;
  display.frame = pulse ? sum - 64 : sum;
  if (!pulse) {
    portEXIT_CRITICAL_ISR(&display.timerMux);
    return;
  }
  const coc::Slot& slot = display.scanSlots[display.publishedPlan][display.isrStep];
  // Preload ALL output latches while outputs remain disabled. This avoids
  // briefly driving a pin with its previous slot's polarity.
  for (uint8_t p = 0; p < coc::kPins; ++p)
    if ((slot.high | slot.low) & (1u << p))
      gpio_set_level((gpio_num_t)scanGpio[p], (slot.high >> p) & 1u);
  asm volatile("nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n");
  // Enable sinks first, then sources. Unused pins stay high impedance.
  for (uint8_t p = 0; p < coc::kPins; ++p)
    if (slot.low & (1u << p))
      gpio_set_direction((gpio_num_t)scanGpio[p], GPIO_MODE_OUTPUT);
  for (uint8_t p = 0; p < coc::kPins; ++p)
    if (slot.high & (1u << p))
      gpio_set_direction((gpio_num_t)scanGpio[p], GPIO_MODE_OUTPUT);
  display.drivenPins = slot.high | slot.low;
  if (++display.isrStep >= MAX_ACTIVE) display.isrStep = 0;
  portEXIT_CRITICAL_ISR(&display.timerMux);
}

CharlieDisplay::CharlieDisplay() {
  brightness = 120;
  effectiveDuty = gammaTable[brightness];
  colonBrightnessLevel = 255;
  colonBrightness = 0;
  colonPwmCounter = 0;
}

void CharlieDisplay::begin() {
  releasePins(63);
  for (uint8_t p = 0; p < coc::kPins; ++p)
    gpio_set_pull_mode((gpio_num_t)scanGpio[p], GPIO_FLOATING);
  gpio_set_level((gpio_num_t)COLON_PIN, 0);
  gpio_set_direction((gpio_num_t)COLON_PIN, GPIO_MODE_OUTPUT);
  timer = timerBegin(1000000);
  timerAttachInterrupt(timer, &onTimer);
  timerAlarm(timer, 40, true, 0);
}

void CharlieDisplay::setBrightness(uint8_t level) {
  portENTER_CRITICAL(&timerMux);
  brightness = level;
  effectiveDuty = gammaTable[level];
  portEXIT_CRITICAL(&timerMux);
}

void CharlieDisplay::setColonBrightness(uint8_t level) {
  portENTER_CRITICAL(&timerMux);
  colonBrightnessLevel = level;
  colonBrightness = activeCount ? gammaTable[level] : 0;
  portEXIT_CRITICAL(&timerMux);
}

CharlieDisplay::ScanStats CharlieDisplay::getScanStats() {
  portENTER_CRITICAL(&timerMux);
  ScanStats result = scanStats;
  portEXIT_CRITICAL(&timerMux);
  return result;
}

void CharlieDisplay::publishCharacters(const uint8_t chars[DIGITS]) {
  uint32_t target = 0;
  for (uint8_t d = 0; d < DIGITS; ++d)
    if (chars[d] < 39)
      for (uint8_t s = 0; s < SEGMENTS; ++s)
        if (font[chars[d]][s]) target |= uint32_t(1) << (d * SEGMENTS + s);
  if (target == publishedTarget) return;

  coc::Limits limits;
  if (COC_ENABLE_GROUPING) {
    limits.total = COC_MAX_SIMULTANEOUS_LEDS;
    limits.perSource = COC_MAX_LEDS_PER_SOURCE;
    limits.perSink = COC_MAX_LEDS_PER_SINK;
  }
  limits.searchNodes = COC_SEARCH_NODE_LIMIT;
  coc::Plan plan = planner.build(target, limits);
  bool fallback = !coc::verify(plan, target, limits);
  if (fallback) {
    limits.total = limits.perSource = limits.perSink = 1;
    plan = planner.build(target, limits);
  }
  // A failed fallback publishes a blank frame rather than an unchecked plan.
  if (!coc::verify(plan, target, limits)) plan = coc::Plan{};
  const uint8_t next = publishedPlan ^ 1u;
  for (uint8_t q = 0; q < MAX_ACTIVE; ++q) scanSlots[next][q] = coc::Slot{};
  for (uint8_t i = 0; i < plan.count; ++i) {
    const uint8_t quantum = uint16_t(i) * MAX_ACTIVE / plan.count;
    scanSlots[next][quantum] = plan.slots[i];
  }
  portENTER_CRITICAL(&timerMux);
  publishedPlan = next;
  publishedTarget = target;
  activeCount = plan.lit;
  scanStats = {plan.lit, plan.count, plan.nodes, plan.optimal,
               fallback || plan.fallback, COC_ENABLE_GROUPING != 0};
  for (uint8_t d = 0; d < DIGITS; ++d) charBuffer[d] = chars[d];
  colonBrightness = activeCount ? gammaTable[colonBrightnessLevel] : 0;
  isrStep = 0;
  portEXIT_CRITICAL(&timerMux);
}

void CharlieDisplay::update(struct tm* t) {
  if (!t) return;
  const int h = t->tm_hour, m = t->tm_min;
  const uint8_t chars[DIGITS] = {
    uint8_t(h < 10 ? 255 : charToIndex('0' + h / 10)),
    charToIndex('0' + h % 10), charToIndex('0' + m / 10),
    charToIndex('0' + m % 10)
  };
  publishCharacters(chars);
}

void CharlieDisplay::print(const char* str) {
  uint8_t chars[DIGITS] = {255,255,255,255};
  if (str)
    for (uint8_t i = 0; i < DIGITS && str[i]; ++i) chars[i] = charToIndex(str[i]);
  publishCharacters(chars);
}

void CharlieDisplay::setColon(bool on) {

  portENTER_CRITICAL(&timerMux);

  colonEnabled = on;

  if (!on) {
    gpio_set_level((gpio_num_t)COLON_PIN, 0);
  }

  portEXIT_CRITICAL(&timerMux);
}

void CharlieDisplay::setColonBlink(bool enable) {

  portENTER_CRITICAL(&timerMux);
  colonBlink = enable;
  colonBlinkTicks = 0;
  portEXIT_CRITICAL(&timerMux);
}

// -------------------------------------------------
uint8_t CharlieDisplay::charToIndex(char c) {

  uint8_t uc = (uint8_t)c;

  if (uc == 36) return 36;

  if (c >= '0' && c <= '9') return c - '0';

  if (c >= 'A' && c <= 'Z') return c - 'A' + 10;

  if (c == '-') return 37;
  if (c == ' ') return 38;

  return 38;
}
