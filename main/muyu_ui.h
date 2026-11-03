#pragma once

#include "muyu_model.h"

typedef struct {
    int battery_percent; // -1 means unavailable.
    bool audio_ok;
    bool storage_ok;
    bool saved;
    bool buttons_ok;
} muyu_status_t;

// All calls run with the BSP LVGL lock held. Objects live for the application.
void muyu_ui_create(void);
void muyu_ui_update(const muyu_model_t *model, const muyu_status_t *status);
void muyu_ui_strike(uint64_t now_ms);
void muyu_ui_animate(uint64_t now_ms);
