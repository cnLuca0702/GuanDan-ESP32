#include "guandan_model.h"

static const char *const LEVELS[GD_LEVEL_COUNT] = {
    "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K",
    "A1", "A2", "A3",
};

static uint8_t clamp_level(uint8_t value) {
    return value > GD_LEVEL_MAX ? GD_LEVEL_MAX : value;
}

void guandan_model_init(guandan_model_t *model, uint8_t red, uint8_t blue) {
    if (!model) return;
    *model = (guandan_model_t) { .red = clamp_level(red), .blue = clamp_level(blue) };
}

const char *guandan_level_label(uint8_t level) {
    return LEVELS[clamp_level(level)];
}

static void dismiss_reset(guandan_model_t *model) {
    model->reset_pending = false;
    model->reset_started_ms = 0;
}

guandan_result_t guandan_model_tick(guandan_model_t *model, uint64_t now_ms) {
    guandan_result_t result = {0};
    if (model && model->reset_pending && now_ms >= model->reset_started_ms
        && now_ms - model->reset_started_ms >= GD_RESET_TIMEOUT_MS) {
        dismiss_reset(model);
        result.view_changed = true;
    }
    return result;
}

guandan_result_t guandan_model_input(guandan_model_t *model,
                                     guandan_action_t action, uint64_t now_ms) {
    guandan_result_t result = {0};
    if (!model) return result;
    // Cancellation has no score meaning, even if the timeout already elapsed
    // before the application's queued press was handled.
    if (action == GD_CANCEL_RESET) {
        if (model->reset_pending) {
            dismiss_reset(model);
            result.view_changed = true;
        }
        return result;
    }
    // An input timestamp before this confirmation opened cannot confirm it.
    if (model->reset_pending && now_ms < model->reset_started_ms) return result;
    result = guandan_model_tick(model, now_ms);
    if (model->reset_pending) {
        if (action == GD_OK) {
            result.scores_changed = model->red != 0 || model->blue != 0;
            model->red = model->blue = 0;
            dismiss_reset(model);
            result.view_changed = true;
        } else if (action == GD_RED_UP || action == GD_RED_DOWN
                   || action == GD_BLUE_UP || action == GD_BLUE_DOWN) {
            // A score key cancels this modal; it never also changes a score.
            dismiss_reset(model);
            result.view_changed = true;
        }
        return result;
    }

    uint8_t before_red = model->red, before_blue = model->blue;
    switch (action) {
    case GD_RED_UP:
        if (model->red < GD_LEVEL_MAX) ++model->red;
        break;
    case GD_RED_DOWN:
        if (model->red > 0) --model->red;
        break;
    case GD_BLUE_UP:
        if (model->blue < GD_LEVEL_MAX) ++model->blue;
        break;
    case GD_BLUE_DOWN:
        if (model->blue > 0) --model->blue;
        break;
    case GD_OK:
        model->reset_pending = true;
        model->reset_started_ms = now_ms;
        result.view_changed = true;
        break;
    case GD_NONE:
    case GD_CANCEL_RESET:
    default:
        break;
    }
    result.scores_changed = model->red != before_red || model->blue != before_blue;
    result.view_changed = result.view_changed || result.scores_changed;
    return result;
}
