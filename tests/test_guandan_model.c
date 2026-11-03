#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "guandan_model.h"

static void no_change(guandan_result_t result) {
    assert(!result.scores_changed && !result.view_changed);
}

static guandan_model_t example(void) {
    guandan_model_t model;
    guandan_model_init(&model, 4, 9);
    return model;
}

static void test_labels_and_restore(void) {
    const char *expected[] = {
        "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K",
        "A1", "A2", "A3",
    };
    assert(sizeof(expected) / sizeof(expected[0]) == GD_LEVEL_COUNT);
    for (uint8_t i = 0; i < GD_LEVEL_COUNT; ++i)
        assert(strcmp(guandan_level_label(i), expected[i]) == 0);
    assert(strcmp(guandan_level_label(255), "A3") == 0);
    guandan_model_t model;
    guandan_model_init(&model, 8, 12);
    assert(model.red == 8 && model.blue == 12 && !model.reset_pending);
    assert(model.reset_started_ms == 0);
    guandan_model_init(&model, 255, 15);
    assert(model.red == GD_LEVEL_MAX && model.blue == GD_LEVEL_MAX);
}

static void test_levels_and_clamping(void) {
    guandan_model_t model;
    guandan_model_init(&model, 0, 0);
    no_change(guandan_model_input(&model, GD_RED_DOWN, 0));
    no_change(guandan_model_input(&model, GD_BLUE_DOWN, 0));
    for (uint8_t i = 1; i < GD_LEVEL_COUNT; ++i) {
        guandan_result_t red = guandan_model_input(&model, GD_RED_UP, i);
        assert(red.scores_changed && red.view_changed);
        assert(model.red == i && model.blue == i - 1);
        guandan_result_t blue = guandan_model_input(&model, GD_BLUE_UP, i);
        assert(blue.scores_changed && blue.view_changed);
        assert(model.red == i && model.blue == i);
    }
    no_change(guandan_model_input(&model, GD_RED_UP, 20));
    no_change(guandan_model_input(&model, GD_BLUE_UP, 20));
    assert(model.red == GD_LEVEL_MAX && model.blue == GD_LEVEL_MAX);
    for (uint8_t i = GD_LEVEL_MAX; i > 0; --i) {
        assert(guandan_model_input(&model, GD_RED_DOWN, 30 + GD_LEVEL_MAX - i).scores_changed);
        assert(guandan_model_input(&model, GD_BLUE_DOWN, 30 + GD_LEVEL_MAX - i).scores_changed);
        assert(model.red == i - 1 && model.blue == i - 1);
    }
    no_change(guandan_model_input(&model, GD_RED_DOWN, 50));
    no_change(guandan_model_input(&model, GD_BLUE_DOWN, 50));
}

static void test_two_ok_confirmation(void) {
    guandan_model_t model = example();
    guandan_result_t result = guandan_model_input(&model, GD_OK, 100);
    assert(result.view_changed && !result.scores_changed);
    assert(model.reset_pending && model.reset_started_ms == 100);
    assert(model.red == 4 && model.blue == 9);
    result = guandan_model_input(&model, GD_OK, 101);
    assert(result.scores_changed && result.view_changed);
    assert(model.red == 0 && model.blue == 0 && !model.reset_pending);
    assert(model.reset_started_ms == 0);
}

static void test_confirm_already_two(void) {
    guandan_model_t model;
    guandan_model_init(&model, 0, 0);
    guandan_model_input(&model, GD_OK, 10);
    guandan_result_t result = guandan_model_input(&model, GD_OK, 20);
    assert(result.view_changed && !result.scores_changed && !model.reset_pending);
}

static void test_each_score_action_cancels_without_scoring(void) {
    const guandan_action_t actions[] = {GD_RED_UP, GD_RED_DOWN, GD_BLUE_UP, GD_BLUE_DOWN};
    for (size_t i = 0; i < sizeof(actions) / sizeof(actions[0]); ++i) {
        guandan_model_t model = example();
        guandan_model_input(&model, GD_OK, 100);
        guandan_result_t result = guandan_model_input(&model, actions[i], 200);
        assert(result.view_changed && !result.scores_changed);
        assert(!model.reset_pending && model.red == 4 && model.blue == 9);
    }
}

static void test_timeout_boundary(void) {
    guandan_model_t model = example();
    guandan_model_input(&model, GD_OK, 100);
    no_change(guandan_model_tick(&model, 8099));
    assert(model.reset_pending);
    guandan_result_t result = guandan_model_tick(&model, 8100);
    assert(result.view_changed && !result.scores_changed);
    assert(!model.reset_pending && model.red == 4 && model.blue == 9);
    no_change(guandan_model_tick(&model, 8101));
}

static void test_expired_ok_opens_new_confirmation(void) {
    guandan_model_t model = example();
    guandan_model_input(&model, GD_OK, 100);
    guandan_result_t result = guandan_model_input(&model, GD_OK, 8100);
    assert(result.view_changed && !result.scores_changed);
    assert(model.reset_pending && model.reset_started_ms == 8100);
    assert(model.red == 4 && model.blue == 9);
    assert(guandan_model_input(&model, GD_OK, 8101).scores_changed);
    assert(model.red == 0 && model.blue == 0);
}

static void test_score_after_expiry_works_normally(void) {
    guandan_model_t model = example();
    guandan_model_input(&model, GD_OK, 100);
    guandan_result_t result = guandan_model_input(&model, GD_RED_UP, 8100);
    assert(result.view_changed && result.scores_changed);
    assert(!model.reset_pending && model.red == 5 && model.blue == 9);
}

static void test_backward_time_does_not_confirm_or_expire(void) {
    guandan_model_t model = example();
    guandan_model_input(&model, GD_OK, 100);
    no_change(guandan_model_tick(&model, 99));
    no_change(guandan_model_input(&model, GD_OK, 99));
    no_change(guandan_model_input(&model, GD_RED_UP, 99));
    assert(model.reset_pending && model.red == 4 && model.blue == 9);
    assert(guandan_model_input(&model, GD_OK, 101).scores_changed);
}

static void test_explicit_cancel_never_scores(void) {
    guandan_model_t model = example();
    no_change(guandan_model_input(&model, GD_CANCEL_RESET, 0));
    guandan_model_input(&model, GD_OK, 100);
    guandan_result_t result = guandan_model_input(&model, GD_CANCEL_RESET, 8000);
    assert(result.view_changed && !result.scores_changed);
    assert(!model.reset_pending && model.red == 4 && model.blue == 9);
    no_change(guandan_model_input(&model, GD_CANCEL_RESET, 8200));

    // The explicit cancel remains harmless if no periodic timeout ran first.
    guandan_model_input(&model, GD_OK, 10000);
    result = guandan_model_input(&model, GD_CANCEL_RESET, 18100);
    assert(result.view_changed && !result.scores_changed);
    assert(!model.reset_pending && model.red == 4 && model.blue == 9);

    // It is also harmless when the timeout already dismissed the confirmation.
    guandan_model_input(&model, GD_OK, 20000);
    assert(guandan_model_tick(&model, 28000).view_changed);
    no_change(guandan_model_input(&model, GD_CANCEL_RESET, 28100));
    assert(model.red == 4 && model.blue == 9);
}

static void test_unknown_and_null(void) {
    guandan_model_t model = example();
    no_change(guandan_model_input(&model, GD_NONE, 100));
    no_change(guandan_model_input(&model, (guandan_action_t)99, 101));
    guandan_model_input(&model, GD_OK, 102);
    no_change(guandan_model_input(&model, GD_NONE, 103));
    no_change(guandan_model_input(&model, (guandan_action_t)99, 104));
    assert(model.reset_pending);
    no_change(guandan_model_input(NULL, GD_OK, 0));
    no_change(guandan_model_tick(NULL, 0));
    guandan_model_init(NULL, 0, 0);
}

int main(void) {
    test_labels_and_restore();
    test_levels_and_clamping();
    test_two_ok_confirmation();
    test_confirm_already_two();
    test_each_score_action_cancels_without_scoring();
    test_timeout_boundary();
    test_expired_ok_opens_new_confirmation();
    test_score_after_expiry_works_normally();
    test_backward_time_does_not_confirm_or_expire();
    test_explicit_cancel_never_scores();
    test_unknown_and_null();
    puts("Guandan model: 11 host scenarios passed");
    return 0;
}
