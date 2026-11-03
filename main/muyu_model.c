#include "muyu_model.h"

static const uint8_t VOLUMES[MUYU_VOLUME_COUNT] = { 0, 25, 50, 75 };
static const uint16_t TEMPOS[MUYU_TEMPO_COUNT] = { 60, 90, 120 };

void muyu_model_init(muyu_model_t *model, uint32_t saved_total,
                     uint8_t volume_index, uint8_t tempo_index, uint64_t now_ms) {
    if (!model) return;
    *model = (muyu_model_t) {
        .total = saved_total,
        .volume_index = volume_index < MUYU_VOLUME_COUNT
                      ? volume_index : MUYU_DEFAULT_VOLUME_INDEX,
        .tempo_index = tempo_index < MUYU_TEMPO_COUNT
                     ? tempo_index : MUYU_DEFAULT_TEMPO_INDEX,
        .last_auto_ms = now_ms,
    };
}

uint8_t muyu_model_volume_percent(const muyu_model_t *model) {
    uint8_t index = model && model->volume_index < MUYU_VOLUME_COUNT
                  ? model->volume_index : MUYU_DEFAULT_VOLUME_INDEX;
    return VOLUMES[index];
}

uint16_t muyu_model_bpm(const muyu_model_t *model) {
    uint8_t index = model && model->tempo_index < MUYU_TEMPO_COUNT
                  ? model->tempo_index : MUYU_DEFAULT_TEMPO_INDEX;
    return TEMPOS[index];
}

uint32_t muyu_model_interval_ms(const muyu_model_t *model) {
    uint32_t bpm = muyu_model_bpm(model);
    return (60000u + bpm / 2u) / bpm;
}

static void strike(muyu_model_t *model, muyu_result_t *result) {
    result->strike = true;
    result->state_changed = true;
    if (model->total < UINT32_MAX) {
        ++model->total;
        result->total_changed = true;
    }
    if (model->session < UINT32_MAX) ++model->session;
}

static void expire_confirmation(muyu_model_t *model, uint64_t now_ms,
                                muyu_result_t *result) {
    if (model->reset_confirm
        && now_ms - model->reset_started_ms >= MUYU_RESET_TIMEOUT_MS) {
        model->reset_confirm = false;
        result->state_changed = true;
    }
}

muyu_result_t muyu_model_input(muyu_model_t *model, muyu_input_t input,
                               uint64_t now_ms) {
    muyu_result_t result = {0};
    if (!model) return result;
    expire_confirmation(model, now_ms, &result);

    if (model->reset_confirm) {
        if (input == MUYU_OK_PRESS) {
            model->session = 0;
            model->reset_confirm = false;
            result.state_changed = true;
        } else if (input == MUYU_UP_SHORT || input == MUYU_UP_LONG
                   || input == MUYU_DOWN_SHORT || input == MUYU_DOWN_LONG) {
            model->reset_confirm = false;
            result.state_changed = true;
        }
        return result;
    }

    switch (input) {
    case MUYU_OK_PRESS:
        strike(model, &result);
        break;
    case MUYU_UP_SHORT:
        model->auto_enabled = !model->auto_enabled;
        model->last_auto_ms = now_ms;
        result.state_changed = true;
        break;
    case MUYU_UP_LONG:
        model->tempo_index = (uint8_t)((model->tempo_index + 1u) % MUYU_TEMPO_COUNT);
        model->last_auto_ms = now_ms;
        result.settings_changed = true;
        result.state_changed = true;
        break;
    case MUYU_DOWN_SHORT:
        model->volume_index = (uint8_t)((model->volume_index + 1u) % MUYU_VOLUME_COUNT);
        result.settings_changed = true;
        result.state_changed = true;
        break;
    case MUYU_DOWN_LONG:
        model->auto_enabled = false;
        model->reset_confirm = true;
        model->reset_started_ms = now_ms;
        result.state_changed = true;
        break;
    case MUYU_INPUT_NONE:
    default:
        break;
    }
    return result;
}

muyu_result_t muyu_model_tick(muyu_model_t *model, uint64_t now_ms) {
    muyu_result_t result = {0};
    if (!model) return result;
    expire_confirmation(model, now_ms, &result);
    if (model->auto_enabled && !model->reset_confirm
        && now_ms - model->last_auto_ms >= muyu_model_interval_ms(model)) {
        // A delayed worker emits only one strike. Never replay a backlog of
        // sounds or award unobserved strikes after a scheduling stall.
        model->last_auto_ms = now_ms;
        strike(model, &result);
    }
    return result;
}
