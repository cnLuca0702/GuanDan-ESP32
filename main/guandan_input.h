#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "guandan_model.h"

#define GD_LONG_PRESS_MS UINT64_C(650)

typedef enum {
    GD_KEY_UP = 0,
    GD_KEY_DOWN = 1,
    GD_KEY_OK = 2,
} guandan_key_t;

typedef enum {
    GD_EDGE_PRESS = 0,
    GD_EDGE_RELEASE = 1,
} guandan_edge_t;

typedef struct {
    int active_key;
    uint64_t pressed_at_ms;
    uint64_t last_edge_ms;
    uint64_t last_tick_ms;
    bool have_edge_time;
    bool have_tick_time;
    bool long_fired;
    bool ignore_until_release;
} guandan_input_state_t;

// Initialize/reset before use. true discards a wake/overflow gesture through
// release. A hardware-idle sample may reset with false if no release will arrive.
void guandan_input_reset(guandan_input_state_t *state, bool ignore_until_release);

// Feed only timestamped PRESS/RELEASE edges; never also feed BSP CLICK/LONG.
// UP controls red, DOWN blue. A short release adds; a held key subtracts once
// at 650 ms via tick, or on release if tick was late. OK emits only on a short
// release, so one continuous hold cannot perform two reset confirmations.
//
// Duplicate presses and wrong releases are ignored. A different PRESS before
// the active RELEASE contaminates the gesture: discard all its actions until
// the most recently pressed key releases. Stale releases of earlier keys do
// not end that quarantine. Backward edge timestamps also discard the gesture.
guandan_action_t guandan_input_handle(guandan_input_state_t *state,
                                      guandan_key_t key, guandan_edge_t edge,
                                      uint64_t timestamp_ms);

// Call periodically after draining available edges. Tick time is tracked
// separately: a queued edge may legitimately predate the previous tick.
// Backward ticks are ignored. All access belongs to one application task.
guandan_action_t guandan_input_tick(guandan_input_state_t *state, uint64_t now_ms);
