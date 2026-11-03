#pragma once

#include <stdbool.h>
#include <stdint.h>

#define MUYU_VOLUME_COUNT 4u
#define MUYU_TEMPO_COUNT 3u
#define MUYU_DEFAULT_VOLUME_INDEX 2u
#define MUYU_DEFAULT_TEMPO_INDEX 0u
#define MUYU_RESET_TIMEOUT_MS UINT64_C(10000)

typedef enum {
    MUYU_INPUT_NONE = 0,
    MUYU_OK_PRESS,
    MUYU_UP_SHORT,
    MUYU_UP_LONG,
    MUYU_DOWN_SHORT,
    MUYU_DOWN_LONG,
} muyu_input_t;

typedef struct {
    uint32_t total;
    uint32_t session;
    uint8_t volume_index;
    uint8_t tempo_index;
    bool auto_enabled;
    bool reset_confirm;
    uint64_t last_auto_ms;
    uint64_t reset_started_ms;
} muyu_model_t;

typedef struct {
    bool strike;
    bool settings_changed;
    bool total_changed;
    bool state_changed;
} muyu_result_t;

// Time arguments use one monotonic millisecond clock. The caller serializes all
// model access. This model performs no I/O and allocates no memory.
void muyu_model_init(muyu_model_t *model, uint32_t saved_total,
                     uint8_t volume_index, uint8_t tempo_index, uint64_t now_ms);
muyu_result_t muyu_model_input(muyu_model_t *model, muyu_input_t input,
                               uint64_t now_ms);
muyu_result_t muyu_model_tick(muyu_model_t *model, uint64_t now_ms);

uint8_t muyu_model_volume_percent(const muyu_model_t *model);
uint16_t muyu_model_bpm(const muyu_model_t *model);
uint32_t muyu_model_interval_ms(const muyu_model_t *model);
