#include "muyu_input.h"

void muyu_input_reset(muyu_input_state_t *state, bool ignore_until_release) {
    if (!state) return;
    *state = (muyu_input_state_t) {
        .active_key = -1,
        .ignore_until_release = ignore_until_release,
    };
}

muyu_input_t muyu_input_handle(muyu_input_state_t *state, muyu_key_t key,
                               muyu_edge_t edge, uint64_t timestamp_ms) {
    if (!state || key < MUYU_KEY_UP || key > MUYU_KEY_OK
        || (edge != MUYU_EDGE_PRESS && edge != MUYU_EDGE_RELEASE)) {
        return MUYU_INPUT_NONE;
    }

    if (state->ignore_until_release) {
        if (edge == MUYU_EDGE_RELEASE) muyu_input_reset(state, false);
        return MUYU_INPUT_NONE;
    }

    if (edge == MUYU_EDGE_PRESS) {
        if (state->active_key == (int)key) return MUYU_INPUT_NONE;
        // Only one ladder key is tracked. A new key cancels any unfinished
        // UP/DOWN action instead of fabricating a release for the old key.
        state->active_key = (int)key;
        state->pressed_at_ms = timestamp_ms;
        return key == MUYU_KEY_OK ? MUYU_OK_PRESS : MUYU_INPUT_NONE;
    }

    if (state->active_key != (int)key) return MUYU_INPUT_NONE;
    bool long_press = timestamp_ms - state->pressed_at_ms >= MUYU_LONG_PRESS_MS;
    state->active_key = -1;
    if (key == MUYU_KEY_UP) return long_press ? MUYU_UP_LONG : MUYU_UP_SHORT;
    if (key == MUYU_KEY_DOWN) return long_press ? MUYU_DOWN_LONG : MUYU_DOWN_SHORT;
    return MUYU_INPUT_NONE;
}
