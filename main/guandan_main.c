// Offline Guandan scoreboard. The main task owns the model and LVGL calls;
// the I/O worker owns NVS and the battery gauge after startup.
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_pins.h"
#include "guandan_input.h"
#include "guandan_model.h"
#include "guandan_ui.h"
#include "key_guard.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <stdatomic.h>

#define STORE_SCHEMA 1u
#define SAVE_IDLE_MS 500u
#define SAVE_MAX_MS 5000u
#define DIM_IDLE_MS 180000u
#define INPUT_QUEUE_LENGTH 24u

static const char *TAG = "guandan";
static QueueHandle_t s_input_queue, s_store_queue, s_status_queue;
static atomic_bool s_input_ready, s_input_overflow;
static key_guard_t s_key_guard;
static nvs_handle_t s_nvs;
static bool s_storage_open;

typedef struct {
    uint32_t schema;
    uint8_t red, blue;
    uint8_t reserved[2];
} stored_state_t;
_Static_assert(sizeof(stored_state_t) == 8, "Scoreboard record layout changed");

typedef struct { bsp_btn_t key; bsp_btn_ev_t edge; uint64_t ms; } input_event_t;
typedef struct { stored_state_t state; uint32_t revision; } store_request_t;
typedef struct {
    int battery_percent;
    bool storage_ok;
    uint32_t saved_revision;
} io_status_t;

static uint64_t now_ms(void) { return (uint64_t)esp_timer_get_time() / 1000; }

static void startup_cleanup(void) {
    QueueHandle_t *queues[] = { &s_input_queue, &s_store_queue, &s_status_queue };
    for (unsigned i = 0; i < sizeof(queues) / sizeof(queues[0]); ++i) {
        if (*queues[i]) vQueueDelete(*queues[i]);
        *queues[i] = NULL;
    }
    if (s_storage_open) nvs_close(s_nvs);
    s_storage_open = false;
}

static stored_state_t load_state(void) {
    stored_state_t initial = { .schema = STORE_SCHEMA };
    esp_err_t err = nvs_flash_init();
    // In particular, never erase another application's NVS to recover an error.
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS unavailable (no erase): %s", esp_err_to_name(err));
        return initial;
    }
    err = nvs_open("guandan_v1", NVS_READWRITE, &s_nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Score storage unavailable: %s", esp_err_to_name(err));
        return initial;
    }
    stored_state_t saved;
    size_t size = sizeof(saved);
    err = nvs_get_blob(s_nvs, "scores", &saved, &size);
    if (err == ESP_OK && size == sizeof(saved) && saved.schema == STORE_SCHEMA
        && saved.red < GD_LEVEL_COUNT && saved.blue < GD_LEVEL_COUNT) {
        s_storage_open = true;
        return saved;
    }
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        s_storage_open = true;
    } else {
        ESP_LOGE(TAG, "Unknown/unreadable score record preserved; using volatile scores");
        nvs_close(s_nvs);
    }
    return initial;
}

static void io_worker(void *arg) {
    (void)arg;
    io_status_t status = { .battery_percent = -1, .storage_ok = s_storage_open };
    xQueueOverwrite(s_status_queue, &status);
    bool battery_ok = bsp_battery_init() == ESP_OK;
    if (battery_ok) status.battery_percent = bsp_battery_soc();
    xQueueOverwrite(s_status_queue, &status);
    store_request_t pending = {0};
    bool dirty = false;
    uint64_t dirty_since = 0, changed_at = 0, retry_at = 0, battery_at = now_ms();
    for (;;) {
        store_request_t update;
        uint64_t now = now_ms();
        if (xQueueReceive(s_store_queue, &update, pdMS_TO_TICKS(50)) == pdTRUE) {
            now = now_ms();
            if (!dirty) dirty_since = now;
            dirty = true;
            pending = update;
            changed_at = now;
        }
        now = now_ms();
        if (dirty && s_storage_open && now >= retry_at
            && (now - changed_at >= SAVE_IDLE_MS || now - dirty_since >= SAVE_MAX_MS)) {
            esp_err_t err = nvs_set_blob(s_nvs, "scores", &pending.state, sizeof(pending.state));
            if (err == ESP_OK) err = nvs_commit(s_nvs);
            status.storage_ok = err == ESP_OK;
            if (status.storage_ok) {
                status.saved_revision = pending.revision;
                dirty = false;
                ESP_LOGI(TAG, "Saved red=%s blue=%s", guandan_level_label(pending.state.red),
                         guandan_level_label(pending.state.blue));
            } else {
                retry_at = now + 5000;
                ESP_LOGE(TAG, "Score save failed: %s", esp_err_to_name(err));
            }
            xQueueOverwrite(s_status_queue, &status);
        }
        if (battery_ok && now - battery_at >= 10000) {
            status.battery_percent = bsp_battery_soc();
            battery_at = now;
            xQueueOverwrite(s_status_queue, &status);
        }
    }
}

// Runs on the shared esp_timer task. Only enqueue edges; no UI, I2C or NVS.
static void on_key(bsp_btn_t key, bsp_btn_ev_t edge, void *user) {
    (void)user;
    if (key_guard_consume(&s_key_guard, edge == BSP_BTN_RELEASE, esp_timer_get_time())) return;
    if (!atomic_load(&s_input_ready) || atomic_load(&s_input_overflow)) return;
    if (edge != BSP_BTN_PRESS && edge != BSP_BTN_RELEASE) return;
    input_event_t event = { .key = key, .edge = edge, .ms = now_ms() };
    if (xQueueSend(s_input_queue, &event, 0) != pdTRUE) atomic_store(&s_input_overflow, true);
}

static void apply_result(const guandan_model_t *model, guandan_result_t result,
                         uint32_t *revision, bool *redraw) {
    *redraw |= result.view_changed || result.scores_changed;
    if (!result.scores_changed) return;
    store_request_t request = {
        .state = { .schema = STORE_SCHEMA, .red = model->red, .blue = model->blue },
        .revision = ++*revision,
    };
    xQueueOverwrite(s_store_queue, &request);
    ESP_LOGI(TAG, "Score red=%s blue=%s", guandan_level_label(model->red),
             guandan_level_label(model->blue));
}

void app_main(void) {
    ESP_LOGI(TAG, "Guandan scoreboard 1.0.0 / offline / ESP32-C3");
    stored_state_t restored = load_state();
    s_input_queue = xQueueCreate(INPUT_QUEUE_LENGTH, sizeof(input_event_t));
    s_store_queue = xQueueCreate(1, sizeof(store_request_t));
    s_status_queue = xQueueCreate(1, sizeof(io_status_t));
    if (!s_input_queue || !s_store_queue || !s_status_queue) {
        ESP_LOGE(TAG, "Application queue allocation failed");
        startup_cleanup();
        return;
    }
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "Display initialization failed");
        startup_cleanup();
        return;
    }
    bsp_display_backlight(80);
    guandan_model_t model;
    guandan_model_init(&model, restored.red, restored.blue);
    guandan_status_t ui = { .battery_percent = -1, .storage_ok = s_storage_open, .saved = true };
    if (!bsp_lvgl_lock(1000)) {
        ESP_LOGE(TAG, "LVGL startup lock timed out");
        startup_cleanup();
        return;
    }
    guandan_ui_create();
    guandan_ui_update(&model, &ui);
    bsp_lvgl_unlock();

    bool worker_ok = xTaskCreate(io_worker, "guandan_io", 4096, NULL, 4, NULL) == pdPASS;
    key_guard_arm(&s_key_guard, esp_timer_get_time(), 10000000);
    ui.buttons_ok = bsp_button_init(on_key, NULL) == ESP_OK;
    if (ui.buttons_ok) key_guard_sync_released(&s_key_guard,
        bsp_button_read_mv() >= BSP_BTN_MV_RELEASED_MIN);
    atomic_store(&s_input_ready, ui.buttons_ok);
    if (!worker_ok) {
        ui.storage_ok = false;
        ESP_LOGE(TAG, "I/O task unavailable; scoring remains usable without persistence");
    }
    guandan_input_state_t input;
    guandan_input_reset(&input, false);
    io_status_t status = { .battery_percent = -1, .storage_ok = worker_ok && s_storage_open };
    uint32_t revision = 0;
    uint64_t last_interaction = now_ms();
    bool dimmed = false, redraw = true;
    ESP_LOGI(TAG, "Ready red=%s blue=%s; heap=%lu largest=%lu",
             guandan_level_label(model.red), guandan_level_label(model.blue),
             (unsigned long)esp_get_free_heap_size(),
             (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    for (;;) {
        if (atomic_load(&s_input_overflow)) {
            xQueueReset(s_input_queue);
            guandan_input_reset(&input, bsp_button_read_mv() < BSP_BTN_MV_RELEASED_MIN);
            atomic_store(&s_input_overflow, false);
            // A lost edge must never confirm a destructive reset.
            if (model.reset_pending) {
                model.reset_pending = false;
                redraw = true;
            }
            ESP_LOGW(TAG, "Input overflow: incomplete gesture discarded");
        }
        input_event_t event;
        for (unsigned i = 0; i < INPUT_QUEUE_LENGTH
             && xQueueReceive(s_input_queue, &event, 0) == pdTRUE; ++i) {
            last_interaction = now_ms();
            if (model.reset_pending && event.edge == BSP_BTN_PRESS
                && (event.key == BSP_BTN_UP || event.key == BSP_BTN_DOWN)) {
                // Cancel on press, then consume this entire gesture. Otherwise
                // a release after the dialog's deadline could change a score.
                apply_result(&model, guandan_model_input(&model, GD_CANCEL_RESET, now_ms()),
                             &revision, &redraw);
                guandan_input_reset(&input, true);
                (void)guandan_input_handle(&input, (guandan_key_t)event.key,
                                          GD_EDGE_PRESS, event.ms);
                continue;
            }
            guandan_action_t action = guandan_input_handle(&input, (guandan_key_t)event.key,
                event.edge == BSP_BTN_PRESS ? GD_EDGE_PRESS : GD_EDGE_RELEASE, event.ms);
            apply_result(&model, guandan_model_input(&model, action, now_ms()), &revision, &redraw);
        }
        uint64_t now = now_ms();
        guandan_action_t held = guandan_input_tick(&input, now);
        apply_result(&model, guandan_model_input(&model, held, now), &revision, &redraw);
        apply_result(&model, guandan_model_tick(&model, now), &revision, &redraw);
        if (xQueueReceive(s_status_queue, &status, 0) == pdTRUE) {
            ui.storage_ok = status.storage_ok;
            ui.battery_percent = status.battery_percent;
            redraw = true;
        }
        bool saved = worker_ok && ui.storage_ok && status.saved_revision == revision;
        if (saved != ui.saved) { ui.saved = saved; redraw = true; }
        // Scores stay readable on the table; waking brightness never eats a tap.
        bool should_dim = !model.reset_pending && now - last_interaction >= DIM_IDLE_MS;
        if (should_dim != dimmed) {
            bsp_display_backlight(should_dim ? 30 : 80);
            dimmed = should_dim;
        }
        if (redraw && bsp_lvgl_lock(20)) {
            guandan_ui_update(&model, &ui);
            bsp_lvgl_unlock();
            redraw = false;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
