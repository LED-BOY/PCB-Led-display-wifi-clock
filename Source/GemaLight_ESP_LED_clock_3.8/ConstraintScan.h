#pragma once
#include <stdint.h>

// Portable planner: physical GPIO numbers and Arduino APIs stay in the driver.
namespace coc {
constexpr uint8_t kPins = 6;
constexpr uint8_t kSegments = 28;
constexpr uint16_t kMaxCandidates = 602;
constexpr uint32_t kAllSegments = (uint32_t(1) << kSegments) - 1;
// Bit positions correspond to GPIO 1, 3, 4, 5, 6, 7, respectively.
constexpr uint8_t kGpio[kPins] = {1, 3, 4, 5, 6, 7};
struct Pair { uint8_t hi, lo; };
constexpr Pair kMap[kSegments] = {
  {0,1},{0,2},{0,3},{0,4},{0,5},{1,0},{3,0},
  {1,2},{1,3},{1,4},{1,5},{2,1},{3,1},{4,1},
  {2,3},{2,4},{2,5},{3,2},{4,2},{5,2},{3,4},
  {3,5},{4,3},{5,3},{4,5},{5,4},{4,0},{2,0}
};
struct Slot {
  uint32_t segments = 0;
  uint8_t high = 0;
  uint8_t low = 0;
};
struct Limits {
  uint8_t total = 1;
  uint8_t perSource = 1;
  uint8_t perSink = 1;
  uint32_t searchNodes = 4096;
};
struct Plan {
  Slot slots[kSegments]{};
  uint8_t count = 0;
  uint8_t lit = 0;
  uint32_t nodes = 0;
  bool optimal = false;  // minimum slot count under this digital model/limits
  bool fallback = false;
};
uint8_t bitCount(uint32_t value);
uint32_t selected(uint8_t high, uint8_t low);
bool valid(const Slot& slot, uint32_t target, const Limits& limits);
bool verify(const Plan& plan, uint32_t target, const Limits& limits);

// Workspace is persistent (~8 KiB); never allocate a Planner on an ESP task stack.
// Non-reentrant: call only from the main task, never from an ISR/network worker.
class Planner {
public:
  Plan build(uint32_t target, const Limits& limits);
private:
  Slot candidates[kMaxCandidates]{};
  Slot path[kSegments]{};
  uint16_t candidateCount = 0;
  uint32_t nodes = 0, budget = 0;
  bool exhausted = false;
  bool search(uint32_t remaining, uint8_t depth, uint8_t slotsLeft, Plan& result);
};
}  // namespace coc
