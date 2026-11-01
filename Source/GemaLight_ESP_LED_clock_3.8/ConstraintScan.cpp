#include "ConstraintScan.h"

namespace coc {
uint8_t bitCount(uint32_t value) {
  uint8_t n = 0;
  while (value) { value &= value - 1; ++n; }
  return n;
}
uint32_t selected(uint8_t high, uint8_t low) {
  uint32_t mask = 0;
  for (uint8_t i = 0; i < kSegments; ++i) {
    if ((high & (1u << kMap[i].hi)) && (low & (1u << kMap[i].lo)))
      mask |= uint32_t(1) << i;
  }
  return mask;
}
bool valid(const Slot& slot, uint32_t target, const Limits& limits) {
  if (((slot.high | slot.low) & ~63u) || (slot.high & slot.low) || !slot.segments)
    return false;
  if (selected(slot.high, slot.low) != slot.segments || (slot.segments & ~target))
    return false;
  if (bitCount(slot.segments) > limits.total) return false;
  uint8_t source[kPins]{}, sink[kPins]{};
  for (uint8_t i = 0; i < kSegments; ++i) {
    if (!(slot.segments & (uint32_t(1) << i))) continue;
    if (++source[kMap[i].hi] > limits.perSource ||
        ++sink[kMap[i].lo] > limits.perSink) return false;
  }
  return true;
}
bool verify(const Plan& plan, uint32_t target, const Limits& limits) {
  if ((target & ~kAllSegments) || plan.count > kSegments ||
      plan.lit != bitCount(target)) return false;
  uint32_t covered = 0;
  for (uint8_t i = 0; i < plan.count; ++i) {
    if (!valid(plan.slots[i], target, limits) || (covered & plan.slots[i].segments))
      return false;
    covered |= plan.slots[i].segments;
  }
  return covered == target;
}
bool Planner::search(uint32_t remaining, uint8_t depth, uint8_t slotsLeft,
                     Plan& result) {
  if (!remaining) {
    result.count = depth;
    for (uint8_t i = 0; i < depth; ++i) result.slots[i] = path[i];
    return true;
  }
  if (!slotsLeft || exhausted) return false;
  if (nodes >= budget) { exhausted = true; return false; }
  ++nodes;
  uint8_t largest = 0;
  for (uint16_t i = 0; i < candidateCount; ++i)
    if (!(candidates[i].segments & ~remaining)) {
      uint8_t n = bitCount(candidates[i].segments);
      if (n > largest) largest = n;
    }
  if (!largest || bitCount(remaining) > slotsLeft * largest) return false;
  const uint32_t first = remaining & (~remaining + 1u);
  for (uint16_t i = 0; i < candidateCount; ++i) {
    const Slot& slot = candidates[i];
    if (!(slot.segments & first) || (slot.segments & ~remaining)) continue;
    path[depth] = slot;
    if (search(remaining ^ slot.segments, depth + 1, slotsLeft - 1, result))
      return true;
    if (exhausted) break;
  }
  return false;
}
Plan Planner::build(uint32_t target, const Limits& limits) {
  target &= kAllSegments;
  Plan result;
  result.lit = bitCount(target);
  if (!target) { result.optimal = true; return result; }
  candidateCount = 0;
  // Enumerate all 3^6 pin assignments, then retain only admissible patterns.
  for (uint16_t code = 0; code < 729; ++code) {
    uint16_t digits = code;
    Slot slot;
    for (uint8_t p = 0; p < kPins; ++p, digits /= 3) {
      if (digits % 3 == 1) slot.high |= 1u << p;
      if (digits % 3 == 2) slot.low |= 1u << p;
    }
    slot.segments = selected(slot.high, slot.low);
    if (!valid(slot, target, limits)) continue;
    // Do not drive pins which do not participate in a selected LED.
    uint8_t used = 0;
    for (uint8_t i = 0; i < kSegments; ++i)
      if (slot.segments & (uint32_t(1) << i))
        used |= (1u << kMap[i].hi) | (1u << kMap[i].lo);
    if (used != (slot.high | slot.low)) continue;
    bool duplicate = false;
    for (uint16_t i = 0; i < candidateCount; ++i)
      if (candidates[i].segments == slot.segments) { duplicate = true; break; }
    if (!duplicate && candidateCount < kMaxCandidates)
      candidates[candidateCount++] = slot;
  }
  // Insertion sort: large groups first, deterministic ties from enumeration.
  for (uint16_t i = 1; i < candidateCount; ++i) {
    Slot value = candidates[i];
    uint16_t j = i;
    while (j && bitCount(candidates[j-1].segments) < bitCount(value.segments)) {
      candidates[j] = candidates[j-1]; --j;
    }
    candidates[j] = value;
  }
  uint32_t remaining = target;
  while (remaining) {
    bool found = false;
    for (uint16_t i = 0; i < candidateCount; ++i) {
      if (candidates[i].segments & ~remaining) continue;
      result.slots[result.count++] = candidates[i];
      remaining ^= candidates[i].segments;
      found = true;
      break;
    }
    if (!found) { result.count = 0; result.fallback = true; return result; }
  }
  nodes = 0; budget = limits.searchNodes; exhausted = false;
  const uint8_t largest = bitCount(candidates[0].segments);
  const uint8_t lower = (result.lit + largest - 1) / largest;
  for (uint8_t n = lower; n < result.count; ++n) {
    if (search(target, 0, n, result)) break;
    if (exhausted) break;
  }
  result.nodes = nodes;
  result.optimal = !exhausted;
  return result;
}
}  // namespace coc
