#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "muyu_model.h"

static muyu_model_t make_model(void) {
    muyu_model_t model;
    muyu_model_init(&model, 42, MUYU_DEFAULT_VOLUME_INDEX,
                     MUYU_DEFAULT_TEMPO_INDEX, 100);
    return model;
}

static void assert_no_effect(muyu_result_t result) {
    assert(!result.strike && !result.total_changed);
    assert(!result.settings_changed && !result.state_changed);
}

static void test_startup_and_restore(void) {
    muyu_model_t model = make_model();
    assert(model.total == 42 && model.session == 0);
    assert(!model.auto_enabled && !model.reset_confirm);
    assert(muyu_model_volume_percent(&model) == 50);
    assert(muyu_model_bpm(&model) == 60);
    assert(muyu_model_interval_ms(&model) == 1000);
    assert_no_effect(muyu_model_tick(&model, 100000));

    muyu_model_init(&model, UINT32_MAX, 255, 255, 0);
    assert(model.total == UINT32_MAX && model.session == 0);
    assert(model.volume_index == MUYU_DEFAULT_VOLUME_INDEX);
    assert(model.tempo_index == MUYU_DEFAULT_TEMPO_INDEX);
    muyu_model_init(&model, 7, 0, 2, 0);
    assert(muyu_model_volume_percent(&model) == 0);
    assert(muyu_model_bpm(&model) == 120);
}

static void test_manual_and_saturation(void) {
    muyu_model_t model = make_model();
    muyu_result_t result = muyu_model_input(&model, MUYU_OK_PRESS, 100);
    assert(result.strike && result.total_changed && result.state_changed);
    assert(!result.settings_changed);
    assert(model.total == 43 && model.session == 1);
    // No implicit repeat on hold: only explicit OK_PRESS inputs strike.
    assert_no_effect(muyu_model_tick(&model, 10100));
    model.total = UINT32_MAX - 1;
    model.session = UINT32_MAX - 1;
    result = muyu_model_input(&model, MUYU_OK_PRESS, 10101);
    assert(result.total_changed);
    assert(model.total == UINT32_MAX && model.session == UINT32_MAX);
    result = muyu_model_input(&model, MUYU_OK_PRESS, 10102);
    assert(result.strike && !result.total_changed);
    assert(model.total == UINT32_MAX && model.session == UINT32_MAX);
}

static void test_auto_timing_and_stalls(void) {
    muyu_model_t model = make_model();
    muyu_result_t result = muyu_model_input(&model, MUYU_UP_SHORT, 500);
    assert(model.auto_enabled && !result.strike && !result.settings_changed);
    assert_no_effect(muyu_model_tick(&model, 1499));
    assert(muyu_model_tick(&model, 1500).strike);
    assert(model.session == 1);
    assert_no_effect(muyu_model_tick(&model, 1500));
    assert(muyu_model_tick(&model, 100000).strike);
    assert(model.session == 2); // One strike, not 98 catch-up strikes.
    assert_no_effect(muyu_model_tick(&model, 100001));
    assert_no_effect(muyu_model_tick(&model, 100999));
    assert(muyu_model_tick(&model, 101000).strike);
    result = muyu_model_input(&model, MUYU_UP_SHORT, 102000);
    assert(!model.auto_enabled && !result.strike);
    assert_no_effect(muyu_model_tick(&model, 102000));
    assert_no_effect(muyu_model_tick(&model, 999999));
}

static void test_tempo_cycle_and_reschedule(void) {
    muyu_model_t model = make_model();
    muyu_model_input(&model, MUYU_UP_SHORT, 100);
    muyu_result_t result = muyu_model_input(&model, MUYU_UP_LONG, 900);
    assert(result.settings_changed && !result.strike);
    assert(muyu_model_bpm(&model) == 90);
    assert(muyu_model_interval_ms(&model) == 667);
    assert_no_effect(muyu_model_tick(&model, 1566));
    assert(muyu_model_tick(&model, 1567).strike);
    muyu_model_input(&model, MUYU_UP_LONG, 1600);
    assert(muyu_model_bpm(&model) == 120);
    assert(muyu_model_interval_ms(&model) == 500);
    assert_no_effect(muyu_model_tick(&model, 2099));
    assert(muyu_model_tick(&model, 2100).strike);
    muyu_model_input(&model, MUYU_UP_LONG, 2200);
    assert(muyu_model_bpm(&model) == 60);
    assert_no_effect(muyu_model_tick(&model, 3199));
    assert(muyu_model_tick(&model, 3200).strike);
}

static void test_manual_during_auto_keeps_schedule(void) {
    muyu_model_t model = make_model();
    muyu_model_input(&model, MUYU_UP_SHORT, 100);
    assert(muyu_model_input(&model, MUYU_OK_PRESS, 600).strike);
    assert(muyu_model_tick(&model, 1100).strike);
    assert(model.total == 44 && model.session == 2);
}

static void test_volume_cycle(void) {
    muyu_model_t model = make_model();
    const uint8_t expected[] = { 75, 0, 25, 50 };
    for (size_t i = 0; i < sizeof(expected); ++i) {
        muyu_result_t result = muyu_model_input(&model, MUYU_DOWN_SHORT, 100 + i);
        assert(result.settings_changed && result.state_changed && !result.strike);
        assert(muyu_model_volume_percent(&model) == expected[i]);
    }
    // Muting sound does not disable strikes or counting.
    model.volume_index = 0;
    assert(muyu_model_input(&model, MUYU_OK_PRESS, 200).strike);
    assert(model.session == 1);
}

static void open_reset(muyu_model_t *model, uint64_t now_ms) {
    muyu_result_t result = muyu_model_input(model, MUYU_DOWN_LONG, now_ms);
    assert(model->reset_confirm && !model->auto_enabled);
    assert(result.state_changed && !result.strike && !result.total_changed);
    assert(!result.settings_changed);
}

static void test_reset_confirmation(void) {
    muyu_model_t model = make_model();
    muyu_model_input(&model, MUYU_OK_PRESS, 101);
    muyu_model_input(&model, MUYU_UP_SHORT, 102);
    open_reset(&model, 103);
    assert_no_effect(muyu_model_tick(&model, 1000));
    muyu_result_t result = muyu_model_input(&model, MUYU_OK_PRESS, 1001);
    assert(result.state_changed && !result.strike && !result.total_changed);
    assert(!result.settings_changed);
    assert(model.session == 0 && model.total == 43);
    assert(!model.reset_confirm && !model.auto_enabled);
    assert_no_effect(muyu_model_tick(&model, 50000));
}

static void test_reset_cancel_actions(void) {
    const muyu_input_t actions[] = {
        MUYU_UP_SHORT, MUYU_UP_LONG, MUYU_DOWN_SHORT, MUYU_DOWN_LONG,
    };
    for (size_t i = 0; i < sizeof(actions) / sizeof(actions[0]); ++i) {
        muyu_model_t model = make_model();
        muyu_model_input(&model, MUYU_OK_PRESS, 101);
        open_reset(&model, 102);
        muyu_result_t result = muyu_model_input(&model, actions[i], 103);
        assert(!model.reset_confirm && !model.auto_enabled);
        assert(model.session == 1 && model.total == 43);
        assert(model.tempo_index == MUYU_DEFAULT_TEMPO_INDEX);
        assert(model.volume_index == MUYU_DEFAULT_VOLUME_INDEX);
        assert(result.state_changed && !result.settings_changed && !result.strike);
    }
}

static void test_reset_timeout(void) {
    muyu_model_t model = make_model();
    muyu_model_input(&model, MUYU_OK_PRESS, 101);
    open_reset(&model, 102);
    assert_no_effect(muyu_model_tick(&model, 10101));
    assert(model.reset_confirm);
    muyu_result_t result = muyu_model_tick(&model, 10102);
    assert(result.state_changed && !result.strike);
    assert(!model.reset_confirm && !model.auto_enabled);
    assert(model.session == 1 && model.total == 43);
    // Input also expires a modal if the periodic worker was delayed.
    open_reset(&model, 20000);
    result = muyu_model_input(&model, MUYU_INPUT_NONE, 30000);
    assert(result.state_changed && !model.reset_confirm);
}

static void test_clock_rollover(void) {
    muyu_model_t model = make_model();
    muyu_model_input(&model, MUYU_UP_SHORT, UINT64_MAX - 499);
    assert_no_effect(muyu_model_tick(&model, 499));
    assert(muyu_model_tick(&model, 500).strike);
    open_reset(&model, UINT64_MAX - 4999);
    assert_no_effect(muyu_model_tick(&model, 4999));
    assert(muyu_model_tick(&model, 5000).state_changed);
    assert(!model.reset_confirm);
}

static void test_ignored_input_and_null(void) {
    muyu_model_t model = make_model();
    assert_no_effect(muyu_model_input(&model, MUYU_INPUT_NONE, 101));
    assert_no_effect(muyu_model_input(&model, (muyu_input_t)99, 102));
    open_reset(&model, 103);
    assert_no_effect(muyu_model_input(&model, (muyu_input_t)99, 104));
    assert(model.reset_confirm);
    assert_no_effect(muyu_model_input(NULL, MUYU_OK_PRESS, 0));
    assert_no_effect(muyu_model_tick(NULL, 0));
    muyu_model_init(NULL, 0, 0, 0, 0);
    assert(muyu_model_volume_percent(NULL) == 50);
    assert(muyu_model_bpm(NULL) == 60);
}

int main(void) {
    test_startup_and_restore();
    test_manual_and_saturation();
    test_auto_timing_and_stalls();
    test_tempo_cycle_and_reschedule();
    test_manual_during_auto_keeps_schedule();
    test_volume_cycle();
    test_reset_confirmation();
    test_reset_cancel_actions();
    test_reset_timeout();
    test_clock_rollover();
    test_ignored_input_and_null();
    puts("Muyu model: 11 host scenarios passed");
    return 0;
}
