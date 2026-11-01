#pragma once

// Experimental multi-LED driving is OFF on the uncharacterized clock PCB.
// Enable only after checking current limiting, source/sink/total current,
// forward voltages and ghosting on the actual hardware. These count limits
// are NOT current regulation or an electrical safety certification.
#ifndef COC_ENABLE_GROUPING
#define COC_ENABLE_GROUPING 0
#endif
#ifndef COC_MAX_SIMULTANEOUS_LEDS
#define COC_MAX_SIMULTANEOUS_LEDS 2
#endif
#ifndef COC_MAX_LEDS_PER_SOURCE
#define COC_MAX_LEDS_PER_SOURCE 2
#endif
#ifndef COC_MAX_LEDS_PER_SINK
#define COC_MAX_LEDS_PER_SINK 2
#endif
#ifndef COC_SEARCH_NODE_LIMIT
#define COC_SEARCH_NODE_LIMIT 4096
#endif

static_assert(COC_ENABLE_GROUPING == 0 || COC_ENABLE_GROUPING == 1,
              "COC_ENABLE_GROUPING must be 0 or 1");
static_assert(COC_MAX_SIMULTANEOUS_LEDS >= 1 && COC_MAX_SIMULTANEOUS_LEDS <= 28,
              "Invalid total LED count limit");
static_assert(COC_MAX_LEDS_PER_SOURCE >= 1 && COC_MAX_LEDS_PER_SOURCE <= 5,
              "Invalid source fanout limit");
static_assert(COC_MAX_LEDS_PER_SINK >= 1 && COC_MAX_LEDS_PER_SINK <= 5,
              "Invalid sink fanin limit");

static_assert(COC_SEARCH_NODE_LIMIT >= 0 && COC_SEARCH_NODE_LIMIT <= 100000,
              "Search node budget must be between 0 and 100000");
