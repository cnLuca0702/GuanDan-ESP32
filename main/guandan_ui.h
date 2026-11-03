#pragma once

#include "guandan_model.h"

typedef struct {
    int battery_percent; // -1 means unavailable.
    bool storage_ok;
    bool saved;
    bool buttons_ok;
} guandan_status_t;

// All calls run with the BSP LVGL lock held. UI objects live for the application.
void guandan_ui_create(void);
void guandan_ui_update(const guandan_model_t *model, const guandan_status_t *status);
