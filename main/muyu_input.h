#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "muyu_model.h"

#define MUYU_LONG_PRESS_MS UINT64_C(500)

typedef enum {
    MUYU_KEY_UP = 0,
    MUYU_KEY_DOWN = 1,
    MUYU_KEY_OK = 2,
} muyu_key_t;

typedef enum {
    MUYU_EDGE_PRESS = 0,
    MUYU_EDGE_RELEASE = 1,
} muyu_edge_t;

typedef struct {
    int active_key;
    uint64_t pressed_at_ms;
    bool ignore_until_release;
} muyu_input_state_t;

// Call before the first event and whenever a pending key must be discarded.
// With ignore_until_release=true, all presses and the first valid release are
// swallowed (for a key held through boot/wake). If hardware is already released
// before callbacks begin, the caller must reset again with false after sampling
// the idle state. This adapter has no timer and does not guess physical release.
void muyu_input_reset(muyu_input_state_t *state, bool ignore_until_release);

// Feed only physical PRESS/RELEASE edges, with their capture-time timestamps
// from a single monotonic millisecond clock, in order. BSP CLICK/DOUBLE/LONG
// events must not also be forwarded. The caller serializes state access.
//
// OK acts on PRESS, at most once until RELEASE. UP/DOWN act on RELEASE, using
// >=500 ms for LONG. If a different key is pressed without the old key being
// released, the old unfinished action is cancelled and the new press starts
// fresh; a late release for the old key is ignored. Duplicate presses neither
// repeat actions nor restart the hold duration. Invalid inputs are ignored.
muyu_input_t muyu_input_handle(muyu_input_state_t *state, muyu_key_t key,
                               muyu_edge_t edge, uint64_t timestamp_ms);
