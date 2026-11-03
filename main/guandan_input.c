#include "guandan_input.h"

void guandan_input_reset(guandan_input_state_t *state, bool ignore_until_release) {
    if (!state) return;
    *state = (guandan_input_state_t) {
        .active_key = -1,
        .ignore_until_release = ignore_until_release,
    };
}

static guandan_action_t long_action(int key) {
    if (key == GD_KEY_UP) return GD_RED_DOWN;
    if (key == GD_KEY_DOWN) return GD_BLUE_DOWN;
    return GD_NONE;
}

static guandan_action_t short_action(int key) {
    if (key == GD_KEY_UP) return GD_RED_UP;
    if (key == GD_KEY_DOWN) return GD_BLUE_UP;
    return GD_OK;
}

guandan_action_t guandan_input_handle(guandan_input_state_t *state,
                                      guandan_key_t key, guandan_edge_t edge,
                                      uint64_t timestamp_ms) {
    if (!state || key < GD_KEY_UP || key > GD_KEY_OK
        || (edge != GD_EDGE_PRESS && edge != GD_EDGE_RELEASE)) return GD_NONE;

    if (state->have_edge_time && timestamp_ms < state->last_edge_ms) {
        state->ignore_until_release = true;
        state->long_fired = false;
        return GD_NONE;
    }
    state->last_edge_ms = timestamp_ms;
    state->have_edge_time = true;

    if (state->ignore_until_release) {
        if (edge == GD_EDGE_PRESS) state->active_key = (int)key;
        else if (state->active_key < 0 || state->active_key == (int)key) {
            state->active_key = -1;
            state->ignore_until_release = false;
            state->long_fired = false;
        }
        return GD_NONE;
    }

    if (edge == GD_EDGE_PRESS) {
        if (state->active_key == (int)key) return GD_NONE;
        if (state->active_key >= 0) {
            state->ignore_until_release = true;
            state->active_key = (int)key;
            state->long_fired = false;
            return GD_NONE;
        }
        state->active_key = (int)key;
        state->pressed_at_ms = timestamp_ms;
        state->long_fired = false;
        return GD_NONE;
    }

    if (state->active_key != (int)key) return GD_NONE;
    state->active_key = -1;
    if (state->long_fired) {
        state->long_fired = false;
        return GD_NONE;
    }
    if (timestamp_ms - state->pressed_at_ms >= GD_LONG_PRESS_MS) return long_action(key);
    return short_action(key);
}

guandan_action_t guandan_input_tick(guandan_input_state_t *state, uint64_t now_ms) {
    if (!state) return GD_NONE;
    if (state->have_tick_time && now_ms < state->last_tick_ms) return GD_NONE;
    state->last_tick_ms = now_ms;
    state->have_tick_time = true;
    if (state->ignore_until_release || state->active_key < 0 || state->long_fired
        || now_ms < state->pressed_at_ms
        || now_ms - state->pressed_at_ms < GD_LONG_PRESS_MS) return GD_NONE;
    state->long_fired = true;
    return long_action(state->active_key);
}
