#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "muyu_input.h"

static muyu_input_state_t make_input(void) {
    muyu_input_state_t input;
    muyu_input_reset(&input, false);
    return input;
}

static void test_short_and_long_boundary(void) {
    const muyu_key_t keys[] = { MUYU_KEY_UP, MUYU_KEY_DOWN };
    const muyu_input_t short_actions[] = { MUYU_UP_SHORT, MUYU_DOWN_SHORT };
    const muyu_input_t long_actions[] = { MUYU_UP_LONG, MUYU_DOWN_LONG };
    for (size_t i = 0; i < 2; ++i) {
        muyu_input_state_t input = make_input();
        assert(muyu_input_handle(&input, keys[i], MUYU_EDGE_PRESS, 100) == MUYU_INPUT_NONE);
        assert(muyu_input_handle(&input, keys[i], MUYU_EDGE_RELEASE, 599) == short_actions[i]);
        assert(muyu_input_handle(&input, keys[i], MUYU_EDGE_PRESS, 600) == MUYU_INPUT_NONE);
        assert(muyu_input_handle(&input, keys[i], MUYU_EDGE_RELEASE, 1100) == long_actions[i]);
        assert(muyu_input_handle(&input, keys[i], MUYU_EDGE_PRESS, 1200) == MUYU_INPUT_NONE);
        assert(muyu_input_handle(&input, keys[i], MUYU_EDGE_RELEASE, 1200) == short_actions[i]);
        assert(muyu_input_handle(&input, keys[i], MUYU_EDGE_RELEASE, 1300) == MUYU_INPUT_NONE);
    }
}

static void test_ok_rapid_strikes_and_no_hold_repeat(void) {
    muyu_input_state_t input = make_input();
    for (uint64_t t = 0; t < 100; t += 2) {
        assert(muyu_input_handle(&input, MUYU_KEY_OK, MUYU_EDGE_PRESS, t) == MUYU_OK_PRESS);
        assert(muyu_input_handle(&input, MUYU_KEY_OK, MUYU_EDGE_PRESS, t) == MUYU_INPUT_NONE);
        assert(muyu_input_handle(&input, MUYU_KEY_OK, MUYU_EDGE_RELEASE, t + 1) == MUYU_INPUT_NONE);
    }
    assert(muyu_input_handle(&input, MUYU_KEY_OK, MUYU_EDGE_PRESS, 100) == MUYU_OK_PRESS);
    assert(muyu_input_handle(&input, MUYU_KEY_OK, MUYU_EDGE_PRESS, 10000) == MUYU_INPUT_NONE);
    assert(muyu_input_handle(&input, MUYU_KEY_OK, MUYU_EDGE_RELEASE, 10001) == MUYU_INPUT_NONE);
}

static void test_duplicate_press_keeps_start_time(void) {
    muyu_input_state_t input = make_input();
    assert(muyu_input_handle(&input, MUYU_KEY_UP, MUYU_EDGE_PRESS, 100) == MUYU_INPUT_NONE);
    assert(muyu_input_handle(&input, MUYU_KEY_UP, MUYU_EDGE_PRESS, 590) == MUYU_INPUT_NONE);
    assert(muyu_input_handle(&input, MUYU_KEY_UP, MUYU_EDGE_RELEASE, 600) == MUYU_UP_LONG);
}

static void test_key_switch_cancels_old_action(void) {
    muyu_input_state_t input = make_input();
    assert(muyu_input_handle(&input, MUYU_KEY_UP, MUYU_EDGE_PRESS, 100) == MUYU_INPUT_NONE);
    assert(muyu_input_handle(&input, MUYU_KEY_DOWN, MUYU_EDGE_PRESS, 1000) == MUYU_INPUT_NONE);
    // The stale UP release cannot generate a long press or cancel DOWN.
    assert(muyu_input_handle(&input, MUYU_KEY_UP, MUYU_EDGE_RELEASE, 1100) == MUYU_INPUT_NONE);
    assert(muyu_input_handle(&input, MUYU_KEY_DOWN, MUYU_EDGE_RELEASE, 1200) == MUYU_DOWN_SHORT);
    assert(muyu_input_handle(&input, MUYU_KEY_UP, MUYU_EDGE_RELEASE, 1201) == MUYU_INPUT_NONE);

    assert(muyu_input_handle(&input, MUYU_KEY_DOWN, MUYU_EDGE_PRESS, 2000) == MUYU_INPUT_NONE);
    assert(muyu_input_handle(&input, MUYU_KEY_OK, MUYU_EDGE_PRESS, 2100) == MUYU_OK_PRESS);
    assert(muyu_input_handle(&input, MUYU_KEY_DOWN, MUYU_EDGE_RELEASE, 3000) == MUYU_INPUT_NONE);
    assert(muyu_input_handle(&input, MUYU_KEY_OK, MUYU_EDGE_PRESS, 3001) == MUYU_INPUT_NONE);
    assert(muyu_input_handle(&input, MUYU_KEY_OK, MUYU_EDGE_RELEASE, 3002) == MUYU_INPUT_NONE);
    assert(muyu_input_handle(&input, MUYU_KEY_OK, MUYU_EDGE_PRESS, 3003) == MUYU_OK_PRESS);
}

static void test_unmatched_release_does_not_cancel_active_key(void) {
    muyu_input_state_t input = make_input();
    assert(muyu_input_handle(&input, MUYU_KEY_UP, MUYU_EDGE_RELEASE, 0) == MUYU_INPUT_NONE);
    assert(muyu_input_handle(&input, MUYU_KEY_UP, MUYU_EDGE_PRESS, 100) == MUYU_INPUT_NONE);
    assert(muyu_input_handle(&input, MUYU_KEY_OK, MUYU_EDGE_RELEASE, 200) == MUYU_INPUT_NONE);
    assert(muyu_input_handle(&input, MUYU_KEY_UP, MUYU_EDGE_RELEASE, 300) == MUYU_UP_SHORT);
}

static void test_reset_discards_pending_hold(void) {
    muyu_input_state_t input = make_input();
    muyu_input_handle(&input, MUYU_KEY_DOWN, MUYU_EDGE_PRESS, 0);
    muyu_input_reset(&input, false);
    assert(muyu_input_handle(&input, MUYU_KEY_DOWN, MUYU_EDGE_RELEASE, 600) == MUYU_INPUT_NONE);
    assert(muyu_input_handle(&input, MUYU_KEY_OK, MUYU_EDGE_PRESS, 700) == MUYU_OK_PRESS);
}

static void test_wake_guard(void) {
    muyu_input_state_t input;
    muyu_input_reset(&input, true);
    assert(muyu_input_handle(&input, MUYU_KEY_OK, MUYU_EDGE_PRESS, 0) == MUYU_INPUT_NONE);
    assert(muyu_input_handle(&input, MUYU_KEY_UP, MUYU_EDGE_PRESS, 100) == MUYU_INPUT_NONE);
    assert(muyu_input_handle(&input, MUYU_KEY_UP, MUYU_EDGE_RELEASE, 10000) == MUYU_INPUT_NONE);
    assert(!input.ignore_until_release);
    assert(muyu_input_handle(&input, MUYU_KEY_UP, MUYU_EDGE_RELEASE, 10001) == MUYU_INPUT_NONE);
    assert(muyu_input_handle(&input, MUYU_KEY_OK, MUYU_EDGE_PRESS, 10002) == MUYU_OK_PRESS);
    // Caller observed an already idle ladder: explicit reset avoids waiting
    // forever for a release that happened before callbacks were installed.
    muyu_input_reset(&input, true);
    muyu_input_reset(&input, false);
    assert(muyu_input_handle(&input, MUYU_KEY_OK, MUYU_EDGE_PRESS, 20000) == MUYU_OK_PRESS);
}

static void test_invalid_inputs(void) {
    muyu_input_state_t input = make_input();
    assert(muyu_input_handle(NULL, MUYU_KEY_OK, MUYU_EDGE_PRESS, 0) == MUYU_INPUT_NONE);
    muyu_input_reset(NULL, false);
    assert(muyu_input_handle(&input, (muyu_key_t)-1, MUYU_EDGE_PRESS, 0) == MUYU_INPUT_NONE);
    assert(muyu_input_handle(&input, (muyu_key_t)3, MUYU_EDGE_PRESS, 0) == MUYU_INPUT_NONE);
    assert(muyu_input_handle(&input, MUYU_KEY_OK, (muyu_edge_t)2, 0) == MUYU_INPUT_NONE);
    assert(input.active_key == -1);
    muyu_input_handle(&input, MUYU_KEY_DOWN, MUYU_EDGE_PRESS, 100);
    assert(muyu_input_handle(&input, (muyu_key_t)3, MUYU_EDGE_RELEASE, 200) == MUYU_INPUT_NONE);
    assert(muyu_input_handle(&input, MUYU_KEY_DOWN, (muyu_edge_t)2, 200) == MUYU_INPUT_NONE);
    assert(muyu_input_handle(&input, MUYU_KEY_DOWN, MUYU_EDGE_RELEASE, 300) == MUYU_DOWN_SHORT);
    muyu_input_reset(&input, true);
    assert(muyu_input_handle(&input, (muyu_key_t)3, MUYU_EDGE_RELEASE, 400) == MUYU_INPUT_NONE);
    assert(input.ignore_until_release);
}

static void test_clock_rollover(void) {
    muyu_input_state_t input = make_input();
    muyu_input_handle(&input, MUYU_KEY_UP, MUYU_EDGE_PRESS, UINT64_MAX - 249);
    assert(muyu_input_handle(&input, MUYU_KEY_UP, MUYU_EDGE_RELEASE, 250) == MUYU_UP_LONG);
}

int main(void) {
    assert(MUYU_KEY_UP == 0 && MUYU_KEY_DOWN == 1 && MUYU_KEY_OK == 2);
    test_short_and_long_boundary();
    test_ok_rapid_strikes_and_no_hold_repeat();
    test_duplicate_press_keeps_start_time();
    test_key_switch_cancels_old_action();
    test_unmatched_release_does_not_cancel_active_key();
    test_reset_discards_pending_hold();
    test_wake_guard();
    test_invalid_inputs();
    test_clock_rollover();
    puts("Muyu input: 9 host scenarios passed");
    return 0;
}
