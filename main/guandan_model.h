#pragma once

#include <stdbool.h>
#include <stdint.h>

#define GD_LEVEL_COUNT 15u
#define GD_LEVEL_MAX (GD_LEVEL_COUNT - 1u)
#define GD_RESET_TIMEOUT_MS UINT64_C(8000)

typedef enum {
    GD_NONE = 0,
    GD_RED_UP,
    GD_RED_DOWN,
    GD_BLUE_UP,
    GD_BLUE_DOWN,
    GD_OK,
    GD_CANCEL_RESET,
} guandan_action_t;

typedef struct {
    uint8_t red;
    uint8_t blue;
    bool reset_pending;
    uint64_t reset_started_ms;
} guandan_model_t;

typedef struct {
    bool scores_changed;
    bool view_changed;
} guandan_result_t;

// Levels are indices 0..14: 2..10, J, Q, K, A1, A2, A3. Invalid restored
// indices clamp to A3. No allocation, platform API, or persistent I/O occurs.
void guandan_model_init(guandan_model_t *model, uint8_t red, uint8_t blue);
guandan_result_t guandan_model_input(guandan_model_t *model,
                                     guandan_action_t action, uint64_t now_ms);
guandan_result_t guandan_model_tick(guandan_model_t *model, uint64_t now_ms);
const char *guandan_level_label(uint8_t level);
