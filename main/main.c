// Offline wooden-fish application. UI/model have one owner; the I/O worker
// exclusively owns audio, battery reads and NVS after startup.
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_pins.h"
#include "key_guard.h"
#include "muyu_input.h"
#include "muyu_model.h"
#include "muyu_ui.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <math.h>
#include <stdatomic.h>
#include <string.h>

#define AUDIO_HZ 16000
#define AUDIO_SAMPLES 2560
#define AUDIO_CHUNK 256
#define STORE_SCHEMA 1u
#define SAVE_IDLE_MS 1500u
#define SAVE_MAX_MS 30000u

static const char *TAG = "muyu";
static QueueHandle_t s_input_queue, s_audio_queue, s_store_queue, s_status_queue;
static atomic_bool s_input_ready;
static atomic_bool s_input_overflow;
static key_guard_t s_key_guard;
static int16_t s_tone[AUDIO_SAMPLES];
static nvs_handle_t s_nvs;
static bool s_storage_open;

typedef struct {
    uint32_t schema;
    uint32_t total;
    uint8_t volume;
    uint8_t tempo;
    uint8_t reserved[2];
} stored_state_t;
_Static_assert(sizeof(stored_state_t) == 12, "NVS record layout changed");

typedef struct { bsp_btn_t key; bsp_btn_ev_t edge; uint64_t ms; } input_event_t;
typedef struct { uint64_t ms; uint8_t volume; } audio_request_t;
typedef struct { stored_state_t state; uint32_t revision; } store_request_t;
typedef struct {
    bool audio_ok;
    bool storage_ok;
    int battery_percent;
    uint32_t saved_revision;
} io_status_t;

static uint64_t now_ms(void) { return (uint64_t)esp_timer_get_time() / 1000; }

static void startup_cleanup(void) {
    // Only used before the worker/button callback can own these resources.
    QueueHandle_t *queues[] = { &s_input_queue, &s_audio_queue, &s_store_queue, &s_status_queue };
    for (unsigned i = 0; i < sizeof(queues) / sizeof(queues[0]); ++i) {
        if (*queues[i]) vQueueDelete(*queues[i]);
        *queues[i] = NULL;
    }
    if (s_storage_open) nvs_close(s_nvs);
    s_storage_open = false;
}

static stored_state_t load_state(void) {
    stored_state_t state = { .schema = STORE_SCHEMA,
        .volume = MUYU_DEFAULT_VOLUME_INDEX, .tempo = MUYU_DEFAULT_TEMPO_INDEX };
    esp_err_t err = nvs_flash_init();
    // Do not erase another application's settings on partition/version errors.
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS unavailable (no erase): %s", esp_err_to_name(err));
        return state;
    }
    err = nvs_open("muyu_v1", NVS_READWRITE, &s_nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS namespace unavailable: %s", esp_err_to_name(err));
        return state;
    }
    stored_state_t readback;
    size_t size = sizeof(readback);
    err = nvs_get_blob(s_nvs, "state", &readback, &size);
    if (err == ESP_OK && size == sizeof(readback) && readback.schema == STORE_SCHEMA
        && readback.volume < MUYU_VOLUME_COUNT && readback.tempo < MUYU_TEMPO_COUNT) {
        s_storage_open = true;
        return readback;
    }
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        s_storage_open = true;
    } else {
        // Preserve unknown/corrupt records rather than overwriting them silently.
        ESP_LOGE(TAG, "Saved record unsupported or unreadable; continuing in RAM");
        nvs_close(s_nvs);
    }
    return state;
}

static void make_tone(void) {
    // Original synthesized resonant wood tone; no sampled/licensed recording.
    uint32_t noise = 0x4d555955u;
    for (unsigned i = 0; i < AUDIO_SAMPLES; ++i) {
        float t = (float)i / AUDIO_HZ;
        noise = noise * 1664525u + 1013904223u;
        float click = ((float)(noise >> 16) / 32768.0f - 1.0f) * expf(-600.0f * t);
        float body = 0.62f * sinf(6.2831853f * 690.0f * t) * expf(-29.0f * t)
                   + 0.26f * sinf(6.2831853f * 1120.0f * t) * expf(-42.0f * t)
                   + 0.12f * sinf(6.2831853f * 1830.0f * t) * expf(-65.0f * t);
        float fade = i > AUDIO_SAMPLES - 160 ? (float)(AUDIO_SAMPLES - 1 - i) / 160.0f : 1.0f;
        float attack = i < 16 ? (float)i / 16.0f : 1.0f;
        s_tone[i] = (int16_t)((body + click * 0.18f) * 18000.0f * fade * attack);
    }
}

static void io_worker(void *arg) {
    (void)arg;
    io_status_t status = { .battery_percent = -1, .storage_ok = s_storage_open };
    make_tone();
    status.audio_ok = bsp_audio_init() == ESP_OK;
    if (status.audio_ok) status.audio_ok = bsp_audio_set_format(AUDIO_HZ, 16, 1) == ESP_OK;
    xQueueOverwrite(s_status_queue, &status);
    bool battery_ok = bsp_battery_init() == ESP_OK;
    if (battery_ok) status.battery_percent = bsp_battery_soc();
    xQueueOverwrite(s_status_queue, &status);

    size_t position = AUDIO_SAMPLES;
    static const int16_t silence[AUDIO_CHUNK] = {0};
    store_request_t pending = {0};
    bool dirty = false, silence_due = false;
    uint64_t dirty_since = 0, changed_at = 0, battery_at = now_ms(), retry_at = 0;
    for (;;) {
        uint64_t now = now_ms();
        audio_request_t sound;
        if (xQueueReceive(s_audio_queue, &sound, 0) == pdTRUE) {
            if (sound.volume == 0) {
                if (status.audio_ok) bsp_audio_set_volume(0);
                position = AUDIO_SAMPLES;
                silence_due = status.audio_ok;
            } else if (status.audio_ok && now - sound.ms <= 120) {
                bsp_audio_set_volume(sound.volume);
                position = 0; // Retrigger; never queue a growing backlog of hits.
                silence_due = true;
            }
        }
        store_request_t update;
        if (xQueueReceive(s_store_queue, &update, 0) == pdTRUE) {
            if (!dirty) dirty_since = now;
            dirty = true;
            pending = update;
            changed_at = now;
        }
        if (status.audio_ok && position < AUDIO_SAMPLES) {
            size_t samples = AUDIO_SAMPLES - position;
            if (samples > AUDIO_CHUNK) samples = AUDIO_CHUNK;
            esp_err_t err = bsp_audio_write(s_tone + position, samples * sizeof(int16_t));
            position += samples;
            if (err != ESP_OK) {
                status.audio_ok = false;
                position = AUDIO_SAMPLES;
                ESP_LOGE(TAG, "Audio disabled after write failure: %s", esp_err_to_name(err));
                xQueueOverwrite(s_status_queue, &status);
            }
        } else if (status.audio_ok && silence_due) {
            if (bsp_audio_write(silence, sizeof(silence)) != ESP_OK) status.audio_ok = false;
            silence_due = false;
            xQueueOverwrite(s_status_queue, &status);
        } else {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        now = now_ms();
        bool idle_save = now - changed_at >= SAVE_IDLE_MS && position >= AUDIO_SAMPLES;
        if (dirty && s_storage_open && now >= retry_at
            && (idle_save || now - dirty_since >= SAVE_MAX_MS)) {
            esp_err_t err = nvs_set_blob(s_nvs, "state", &pending.state, sizeof(pending.state));
            if (err == ESP_OK) err = nvs_commit(s_nvs);
            status.storage_ok = err == ESP_OK;
            if (status.storage_ok) {
                status.saved_revision = pending.revision;
                dirty = false;
                ESP_LOGI(TAG, "Saved total=%lu", (unsigned long)pending.state.total);
            } else {
                retry_at = now + 5000;
                ESP_LOGE(TAG, "NVS save failed: %s", esp_err_to_name(err));
            }
            xQueueOverwrite(s_status_queue, &status);
        }
        if (battery_ok && position >= AUDIO_SAMPLES && now - battery_at >= 10000) {
            status.battery_percent = bsp_battery_soc();
            battery_at = now;
            xQueueOverwrite(s_status_queue, &status);
        }
    }
}

// Called on the shared esp_timer task: only timestamp and enqueue. No LVGL,
// audio, I2C, logging, allocation or storage here.
static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user) {
    (void)user;
    if (key_guard_consume(&s_key_guard, ev == BSP_BTN_RELEASE, esp_timer_get_time())) return;
    if (!s_input_ready || atomic_load(&s_input_overflow)) return;
    if (ev != BSP_BTN_PRESS && ev != BSP_BTN_RELEASE) return;
    const input_event_t event = { .key = btn, .edge = ev, .ms = now_ms() };
    if (xQueueSend(s_input_queue, &event, 0) != pdTRUE) atomic_store(&s_input_overflow, true);
}

static void apply_result(const muyu_model_t *model, muyu_result_t result,
                          uint64_t now, uint32_t *revision, bool *hit_pending) {
    if (result.strike) {
        audio_request_t sound = { .ms = now, .volume = muyu_model_volume_percent(model) };
        xQueueOverwrite(s_audio_queue, &sound);
        *hit_pending = true;
    } else if (result.settings_changed && muyu_model_volume_percent(model) == 0) {
        audio_request_t mute = { .ms = now, .volume = 0 };
        xQueueOverwrite(s_audio_queue, &mute);
    }
    if (result.settings_changed || result.total_changed) {
        store_request_t request = {
            .state = { .schema = STORE_SCHEMA, .total = model->total,
                       .volume = model->volume_index, .tempo = model->tempo_index },
            .revision = ++*revision,
        };
        xQueueOverwrite(s_store_queue, &request);
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Muyu 1.0.0 / offline / ESP32-C3");
    stored_state_t restored = load_state();
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "Display initialization failed");
        startup_cleanup();
        return;
    }
    bsp_display_backlight(70);
    s_input_queue = xQueueCreate(24, sizeof(input_event_t));
    s_audio_queue = xQueueCreate(1, sizeof(audio_request_t));
    s_store_queue = xQueueCreate(1, sizeof(store_request_t));
    s_status_queue = xQueueCreate(1, sizeof(io_status_t));
    muyu_model_t model;
    muyu_model_init(&model, restored.total, restored.volume, restored.tempo, now_ms());
    muyu_status_t ui = { .battery_percent = -1, .storage_ok = s_storage_open, .saved = true };
    if (!bsp_lvgl_lock(1000)) {
        ESP_LOGE(TAG, "LVGL startup lock timed out");
        startup_cleanup();
        return;
    }
    muyu_ui_create();
    muyu_ui_update(&model, &ui);
    bsp_lvgl_unlock();
    if (!s_input_queue || !s_audio_queue || !s_store_queue || !s_status_queue) {
        ESP_LOGE(TAG, "Application queue allocation failed");
        startup_cleanup();
        return;
    }
    bool worker_ok = xTaskCreate(io_worker, "muyu_io", 5120, NULL, 4, NULL) == pdPASS;
    // Guard any key held during power-on, including the key that booted the device.
    key_guard_arm(&s_key_guard, esp_timer_get_time(), 10000000);
    ui.buttons_ok = bsp_button_init(on_key, NULL) == ESP_OK;
    if (ui.buttons_ok) key_guard_sync_released(&s_key_guard,
        bsp_button_read_mv() >= BSP_BTN_MV_RELEASED_MIN);
    s_input_ready = ui.buttons_ok;
    if (!worker_ok) {
        ui.audio_ok = false;
        ui.storage_ok = false;
        ESP_LOGE(TAG, "I/O task creation failed; volatile counter remains available");
    }
    muyu_input_state_t adapter;
    muyu_input_reset(&adapter, false);
    io_status_t status = { .storage_ok = worker_ok && s_storage_open, .battery_percent = -1 };
    uint32_t revision = 0;
    uint64_t last_interaction = now_ms();
    bool dimmed = false, hit_pending = false;
    ESP_LOGI(TAG, "Ready; heap=%lu largest=%lu", (unsigned long)esp_get_free_heap_size(),
             (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    for (;;) {
        if (atomic_load(&s_input_overflow)) {
            // Lost edges cannot safely be interpreted as a held button. Stop the
            // producer, discard the partial gesture, then wait for physical idle.
            xQueueReset(s_input_queue);
            muyu_input_reset(&adapter, bsp_button_read_mv() < BSP_BTN_MV_RELEASED_MIN);
            atomic_store(&s_input_overflow, false);
            ESP_LOGW(TAG, "Input overflow: partial gesture discarded");
        }
        input_event_t event;
        for (unsigned count = 0; count < 24 && xQueueReceive(s_input_queue, &event, 0) == pdTRUE; ++count) {
            last_interaction = now_ms();
            muyu_input_t input = muyu_input_handle(&adapter, (muyu_key_t)event.key,
                event.edge == BSP_BTN_PRESS ? MUYU_EDGE_PRESS : MUYU_EDGE_RELEASE, event.ms);
            muyu_result_t result = muyu_model_input(&model, input, now_ms());
            apply_result(&model, result, now_ms(), &revision, &hit_pending);
        }
        uint64_t now = now_ms();
        apply_result(&model, muyu_model_tick(&model, now), now, &revision, &hit_pending);
        if (xQueueReceive(s_status_queue, &status, 0) == pdTRUE) {
            ui.audio_ok = status.audio_ok;
            ui.storage_ok = status.storage_ok;
            ui.battery_percent = status.battery_percent;
        }
        ui.saved = worker_ok && ui.storage_ok && status.saved_revision == revision;
        bool should_dim = !model.auto_enabled && now - last_interaction >= 30000;
        if (should_dim != dimmed) {
            bsp_display_backlight(should_dim ? 10 : 70);
            dimmed = should_dim;
        }
        if (bsp_lvgl_lock(20)) {
            muyu_ui_update(&model, &ui);
            if (hit_pending) {
                muyu_ui_strike(now);
                hit_pending = false;
            }
            muyu_ui_animate(now);
            bsp_lvgl_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
