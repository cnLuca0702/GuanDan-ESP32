#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "guandan_input.h"

static guandan_input_state_t example(void) {
    guandan_input_state_t input;
    guandan_input_reset(&input, false);
    return input;
}

static void test_short_releases(void) {
    const guandan_key_t keys[] = {GD_KEY_UP, GD_KEY_DOWN, GD_KEY_OK};
    const guandan_action_t actions[] = {GD_RED_UP, GD_BLUE_UP, GD_OK};
    for (size_t i = 0; i < 3; ++i) {
        guandan_input_state_t input = example();
        assert(guandan_input_handle(&input, keys[i], GD_EDGE_PRESS, 100) == GD_NONE);
        assert(guandan_input_tick(&input, 748) == GD_NONE);
        assert(guandan_input_handle(&input, keys[i], GD_EDGE_RELEASE, 749) == actions[i]);
        assert(guandan_input_handle(&input, keys[i], GD_EDGE_RELEASE, 750) == GD_NONE);
        assert(guandan_input_tick(&input, 1000) == GD_NONE);
    }
}

static void test_long_tick_once_and_no_release_add(void) {
    const guandan_key_t keys[] = {GD_KEY_UP, GD_KEY_DOWN};
    const guandan_action_t actions[] = {GD_RED_DOWN, GD_BLUE_DOWN};
    for (size_t i = 0; i < 2; ++i) {
        guandan_input_state_t input = example();
        guandan_input_handle(&input, keys[i], GD_EDGE_PRESS, 100);
        assert(guandan_input_tick(&input, 749) == GD_NONE);
        assert(guandan_input_tick(&input, 750) == actions[i]);
        assert(guandan_input_tick(&input, 750) == GD_NONE);
        assert(guandan_input_tick(&input, 2000) == GD_NONE);
        assert(guandan_input_handle(&input, keys[i], GD_EDGE_RELEASE, 2001) == GD_NONE);
        assert(guandan_input_tick(&input, 3000) == GD_NONE);
    }
}

static void test_long_release_when_tick_is_late(void) {
    guandan_input_state_t input = example();
    guandan_input_handle(&input, GD_KEY_UP, GD_EDGE_PRESS, 100);
    assert(guandan_input_handle(&input, GD_KEY_UP, GD_EDGE_RELEASE, 750) == GD_RED_DOWN);
    assert(guandan_input_tick(&input, 751) == GD_NONE);
    guandan_input_handle(&input, GD_KEY_DOWN, GD_EDGE_PRESS, 800);
    assert(guandan_input_handle(&input, GD_KEY_DOWN, GD_EDGE_RELEASE, 1500) == GD_BLUE_DOWN);
    assert(guandan_input_handle(&input, GD_KEY_DOWN, GD_EDGE_RELEASE, 1501) == GD_NONE);
}

static void test_ok_long_never_emits(void) {
    guandan_input_state_t input = example();
    guandan_input_handle(&input, GD_KEY_OK, GD_EDGE_PRESS, 100);
    assert(guandan_input_tick(&input, 750) == GD_NONE);
    assert(guandan_input_tick(&input, 2000) == GD_NONE);
    assert(guandan_input_handle(&input, GD_KEY_OK, GD_EDGE_RELEASE, 2001) == GD_NONE);
    guandan_input_handle(&input, GD_KEY_OK, GD_EDGE_PRESS, 2100);
    assert(guandan_input_handle(&input, GD_KEY_OK, GD_EDGE_RELEASE, 2750) == GD_NONE);
    guandan_input_handle(&input, GD_KEY_OK, GD_EDGE_PRESS, 2800);
    assert(guandan_input_handle(&input, GD_KEY_OK, GD_EDGE_RELEASE, 2850) == GD_OK);
}

static void test_duplicate_press_keeps_original_hold(void) {
    guandan_input_state_t input = example();
    guandan_input_handle(&input, GD_KEY_UP, GD_EDGE_PRESS, 100);
    assert(guandan_input_handle(&input, GD_KEY_UP, GD_EDGE_PRESS, 700) == GD_NONE);
    assert(guandan_input_tick(&input, 750) == GD_RED_DOWN);
    assert(guandan_input_handle(&input, GD_KEY_UP, GD_EDGE_PRESS, 900) == GD_NONE);
    assert(guandan_input_handle(&input, GD_KEY_UP, GD_EDGE_RELEASE, 1000) == GD_NONE);
}

static void test_wrong_release_does_not_complete_gesture(void) {
    guandan_input_state_t input = example();
    assert(guandan_input_handle(&input, GD_KEY_OK, GD_EDGE_RELEASE, 0) == GD_NONE);
    guandan_input_handle(&input, GD_KEY_UP, GD_EDGE_PRESS, 100);
    assert(guandan_input_handle(&input, GD_KEY_DOWN, GD_EDGE_RELEASE, 200) == GD_NONE);
    assert(guandan_input_handle(&input, GD_KEY_UP, GD_EDGE_RELEASE, 300) == GD_RED_UP);
}

static void test_adc_key_switch_discards_both_gestures(void) {
    guandan_input_state_t input = example();
    guandan_input_handle(&input, GD_KEY_UP, GD_EDGE_PRESS, 100);
    assert(guandan_input_handle(&input, GD_KEY_DOWN, GD_EDGE_PRESS, 200) == GD_NONE);
    assert(input.ignore_until_release);
    assert(guandan_input_handle(&input, GD_KEY_UP, GD_EDGE_RELEASE, 300) == GD_NONE);
    assert(input.ignore_until_release);
    assert(guandan_input_tick(&input, 1000) == GD_NONE);
    assert(guandan_input_handle(&input, GD_KEY_DOWN, GD_EDGE_RELEASE, 1100) == GD_NONE);
    assert(!input.ignore_until_release);
    guandan_input_handle(&input, GD_KEY_DOWN, GD_EDGE_PRESS, 1200);
    assert(guandan_input_handle(&input, GD_KEY_DOWN, GD_EDGE_RELEASE, 1250) == GD_BLUE_UP);
}

static void test_switch_recovers_without_old_release(void) {
    guandan_input_state_t input = example();
    guandan_input_handle(&input, GD_KEY_UP, GD_EDGE_PRESS, 100);
    guandan_input_handle(&input, GD_KEY_DOWN, GD_EDGE_PRESS, 200);
    assert(guandan_input_handle(&input, GD_KEY_DOWN, GD_EDGE_RELEASE, 300) == GD_NONE);
    assert(!input.ignore_until_release);
    assert(guandan_input_handle(&input, GD_KEY_UP, GD_EDGE_RELEASE, 350) == GD_NONE);
    guandan_input_handle(&input, GD_KEY_OK, GD_EDGE_PRESS, 400);
    assert(guandan_input_handle(&input, GD_KEY_OK, GD_EDGE_RELEASE, 450) == GD_OK);
}

static void test_three_key_contamination_cannot_confirm(void) {
    guandan_input_state_t input = example();
    guandan_input_handle(&input, GD_KEY_UP, GD_EDGE_PRESS, 100);
    guandan_input_handle(&input, GD_KEY_DOWN, GD_EDGE_PRESS, 200);
    guandan_input_handle(&input, GD_KEY_OK, GD_EDGE_PRESS, 300);
    assert(guandan_input_handle(&input, GD_KEY_DOWN, GD_EDGE_RELEASE, 350) == GD_NONE);
    assert(input.ignore_until_release);
    assert(guandan_input_handle(&input, GD_KEY_OK, GD_EDGE_RELEASE, 400) == GD_NONE);
    assert(!input.ignore_until_release);
}

static void test_backward_edges_cancel_until_valid_release(void) {
    guandan_input_state_t input = example();
    guandan_input_handle(&input, GD_KEY_UP, GD_EDGE_PRESS, 100);
    assert(guandan_input_handle(&input, GD_KEY_UP, GD_EDGE_RELEASE, 99) == GD_NONE);
    assert(input.ignore_until_release);
    assert(guandan_input_tick(&input, 1000) == GD_NONE);
    assert(guandan_input_handle(&input, GD_KEY_UP, GD_EDGE_RELEASE, 110) == GD_NONE);
    assert(!input.ignore_until_release);
    guandan_input_handle(&input, GD_KEY_UP, GD_EDGE_PRESS, 120);
    assert(guandan_input_handle(&input, GD_KEY_UP, GD_EDGE_RELEASE, 130) == GD_RED_UP);
}

static void test_tick_and_edge_clocks_are_separate(void) {
    guandan_input_state_t input = example();
    guandan_input_handle(&input, GD_KEY_UP, GD_EDGE_PRESS, 100);
    assert(guandan_input_tick(&input, 600) == GD_NONE);
    // A queued release captured before the previous tick is still a valid edge.
    assert(guandan_input_handle(&input, GD_KEY_UP, GD_EDGE_RELEASE, 590) == GD_RED_UP);
    assert(!input.ignore_until_release);
    guandan_input_handle(&input, GD_KEY_DOWN, GD_EDGE_PRESS, 700);
    assert(guandan_input_tick(&input, 1350) == GD_BLUE_DOWN);
    assert(guandan_input_tick(&input, 1400) == GD_NONE);
    assert(guandan_input_handle(&input, GD_KEY_DOWN, GD_EDGE_RELEASE, 1360) == GD_NONE);
    assert(!input.ignore_until_release);
}

static void test_backward_ticks_never_underflow(void) {
    guandan_input_state_t input = example();
    guandan_input_handle(&input, GD_KEY_UP, GD_EDGE_PRESS, 100);
    assert(guandan_input_tick(&input, 50) == GD_NONE);
    assert(guandan_input_tick(&input, 200) == GD_NONE);
    assert(guandan_input_tick(&input, 199) == GD_NONE);
    assert(guandan_input_tick(&input, 749) == GD_NONE);
    assert(guandan_input_tick(&input, 750) == GD_RED_DOWN);
}

static void test_wake_and_manual_reset(void) {
    guandan_input_state_t input;
    guandan_input_reset(&input, true);
    guandan_input_handle(&input, GD_KEY_OK, GD_EDGE_PRESS, 100);
    assert(guandan_input_tick(&input, 1000) == GD_NONE);
    assert(guandan_input_handle(&input, GD_KEY_OK, GD_EDGE_RELEASE, 1100) == GD_NONE);
    guandan_input_handle(&input, GD_KEY_UP, GD_EDGE_PRESS, 1200);
    guandan_input_reset(&input, false);
    assert(guandan_input_handle(&input, GD_KEY_UP, GD_EDGE_RELEASE, 1300) == GD_NONE);
    guandan_input_reset(&input, true);
    // Hardware was already idle when callbacks became ready.
    guandan_input_reset(&input, false);
    guandan_input_handle(&input, GD_KEY_UP, GD_EDGE_PRESS, 1400);
    assert(guandan_input_handle(&input, GD_KEY_UP, GD_EDGE_RELEASE, 1450) == GD_RED_UP);
}

static void test_two_independent_short_ok_presses_required(void) {
    guandan_input_state_t input = example();
    guandan_model_t model;
    guandan_model_init(&model, 6, 9);
    guandan_input_handle(&input, GD_KEY_OK, GD_EDGE_PRESS, 100);
    guandan_action_t action = guandan_input_handle(&input, GD_KEY_OK, GD_EDGE_RELEASE, 150);
    assert(action == GD_OK);
    guandan_model_input(&model, action, 150);
    assert(model.reset_pending);
    action = guandan_input_handle(&input, GD_KEY_OK, GD_EDGE_RELEASE, 151);
    guandan_model_input(&model, action, 151);
    assert(model.reset_pending && model.red == 6 && model.blue == 9);
    guandan_input_handle(&input, GD_KEY_OK, GD_EDGE_PRESS, 200);
    action = guandan_input_tick(&input, 850);
    guandan_model_input(&model, action, 850);
    action = guandan_input_handle(&input, GD_KEY_OK, GD_EDGE_RELEASE, 900);
    guandan_model_input(&model, action, 900);
    assert(model.reset_pending && model.red == 6 && model.blue == 9);
    guandan_input_handle(&input, GD_KEY_OK, GD_EDGE_PRESS, 1000);
    action = guandan_input_handle(&input, GD_KEY_OK, GD_EDGE_RELEASE, 1050);
    assert(action == GD_OK);
    guandan_result_t result = guandan_model_input(&model, action, 1050);
    assert(result.scores_changed && !model.reset_pending);
    assert(model.red == 0 && model.blue == 0);
}

static void test_cancel_press_before_timeout_consumes_entire_gesture(void) {
    const guandan_key_t keys[] = {GD_KEY_UP, GD_KEY_DOWN};
    for (size_t i = 0; i < 2; ++i) {
        guandan_input_state_t input = example();
        guandan_model_t model;
        guandan_model_init(&model, 6, 9);
        guandan_model_input(&model, GD_OK, 0);

        // Application routing: a score-key PRESS in the confirmation cancels
        // immediately and quarantines this complete physical gesture.
        assert(model.reset_pending);
        guandan_result_t result = guandan_model_input(&model, GD_CANCEL_RESET, 7900);
        assert(result.view_changed && !result.scores_changed);
        guandan_input_reset(&input, true);
        assert(guandan_input_handle(&input, keys[i], GD_EDGE_PRESS, 7900) == GD_NONE);
        guandan_model_tick(&model, 8000);
        guandan_action_t action = guandan_input_handle(&input, keys[i], GD_EDGE_RELEASE, 8100);
        assert(action == GD_NONE);
        result = guandan_model_input(&model, action, 8100);
        assert(!result.scores_changed && !model.reset_pending);
        assert(model.red == 6 && model.blue == 9);

        // A held cancellation gesture spanning the old deadline cannot subtract.
        guandan_model_input(&model, GD_OK, 10000);
        guandan_model_input(&model, GD_CANCEL_RESET, 17900);
        guandan_input_reset(&input, true);
        guandan_input_handle(&input, keys[i], GD_EDGE_PRESS, 17900);
        guandan_model_tick(&model, 18000);
        action = guandan_input_tick(&input, 18550);
        assert(action == GD_NONE);
        assert(!guandan_model_input(&model, action, 18550).scores_changed);
        action = guandan_input_handle(&input, keys[i], GD_EDGE_RELEASE, 19000);
        assert(action == GD_NONE);
        assert(!guandan_model_input(&model, action, 19000).scores_changed);
        assert(model.red == 6 && model.blue == 9 && !model.reset_pending);

        // Releasing the quarantined key restores the next normal short press.
        guandan_input_handle(&input, keys[i], GD_EDGE_PRESS, 19100);
        action = guandan_input_handle(&input, keys[i], GD_EDGE_RELEASE, 19200);
        assert(action == (keys[i] == GD_KEY_UP ? GD_RED_UP : GD_BLUE_UP));
        assert(guandan_model_input(&model, action, 19200).scores_changed);
    }
}

static void test_invalid_and_null(void) {
    guandan_input_state_t input = example();
    assert(guandan_input_handle(&input, (guandan_key_t)-1, GD_EDGE_PRESS, 0) == GD_NONE);
    assert(guandan_input_handle(&input, (guandan_key_t)3, GD_EDGE_PRESS, 0) == GD_NONE);
    assert(guandan_input_handle(&input, GD_KEY_UP, (guandan_edge_t)2, 0) == GD_NONE);
    assert(input.active_key == -1 && !input.have_edge_time);
    assert(guandan_input_handle(NULL, GD_KEY_OK, GD_EDGE_PRESS, 0) == GD_NONE);
    assert(guandan_input_tick(NULL, 0) == GD_NONE);
    guandan_input_reset(NULL, false);
}

int main(void) {
    assert(GD_KEY_UP == 0 && GD_KEY_DOWN == 1 && GD_KEY_OK == 2);
    test_short_releases();
    test_long_tick_once_and_no_release_add();
    test_long_release_when_tick_is_late();
    test_ok_long_never_emits();
    test_duplicate_press_keeps_original_hold();
    test_wrong_release_does_not_complete_gesture();
    test_adc_key_switch_discards_both_gestures();
    test_switch_recovers_without_old_release();
    test_three_key_contamination_cannot_confirm();
    test_backward_edges_cancel_until_valid_release();
    test_tick_and_edge_clocks_are_separate();
    test_backward_ticks_never_underflow();
    test_wake_and_manual_reset();
    test_two_independent_short_ok_presses_required();
    test_cancel_press_before_timeout_consumes_entire_gesture();
    test_invalid_and_null();
    puts("Guandan input: 16 host scenarios passed");
    return 0;
}
