#include "muyu_ui.h"
#include "lvgl.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(muyu_font_16);
LV_FONT_DECLARE(muyu_font_20);
LV_IMAGE_DECLARE(muyu_woodfish);

#define PAPER 0xF6EEDC
#define INK 0x503626
#define MUTED 0x927A61
#define ACCENT 0xA75330

static lv_obj_t *s_screen, *s_count, *s_total, *s_battery;
static lv_obj_t *s_mode, *s_sound, *s_saved, *s_wood, *s_plus;
static lv_obj_t *s_modal, *s_warning;
static uint64_t s_struck_at;
static bool s_animating;

static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y,
                       int width, const lv_font_t *font, uint32_t color) {
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(obj, width);
    lv_obj_set_pos(obj, x, y);
    lv_label_set_text(obj, text);
    return obj;
}

static void set_text(lv_obj_t *obj, const char *text) {
    // Avoid invalidating static labels on every 20 ms animation tick.
    if (strcmp(lv_label_get_text(obj), text) != 0) lv_label_set_text(obj, text);
}

void muyu_ui_create(void) {
    s_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_screen);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(PAPER), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    label(s_screen, "敲木鱼", 20, 17, 76, &muyu_font_20, INK);
    s_battery = label(s_screen, "--%", 167, 21, 49, &lv_font_montserrat_14, MUTED);
    label(s_screen, "本次功德", 40, 51, 160, &muyu_font_16, MUTED);
    s_count = label(s_screen, "0", 10, 72, 220, &lv_font_montserrat_32, INK);
    s_total = label(s_screen, "累计 0", 10, 110, 220, &muyu_font_16, MUTED);

    s_wood = lv_image_create(s_screen);
    lv_image_set_src(s_wood, &muyu_woodfish);
    lv_obj_set_pos(s_wood, 43, 130);
    s_plus = label(s_screen, "功德 +1", 70, 140, 100, &muyu_font_16, ACCENT);
    lv_obj_set_style_text_opa(s_plus, LV_OPA_TRANSP, 0);

    s_mode = label(s_screen, "手动", 14, 246, 116, &muyu_font_16, ACCENT);
    s_sound = label(s_screen, "声音 50%", 130, 246, 96, &muyu_font_16, MUTED);
    s_saved = label(s_screen, "已保存", 54, 269, 132, &muyu_font_16, MUTED);
    s_warning = label(s_screen, "", 10, 269, 220, &muyu_font_16, ACCENT);
    label(s_screen, "OK 敲击  上自动  下音量", 12, 294, 216, &muyu_font_16, INK);

    // Opaque modal avoids allocating a full-screen alpha compositing layer.
    s_modal = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_modal);
    lv_obj_set_pos(s_modal, 16, 80);
    lv_obj_set_size(s_modal, 208, 154);
    lv_obj_remove_flag(s_modal, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(s_modal, 18, 0);
    lv_obj_set_style_bg_color(s_modal, lv_color_hex(0xFFF9EE), 0);
    lv_obj_set_style_bg_opa(s_modal, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_modal, 2, 0);
    lv_obj_set_style_border_color(s_modal, lv_color_hex(0xC9AA7F), 0);
    label(s_modal, "清空本次?", 8, 18, 192, &muyu_font_20, INK);
    label(s_modal, "累计保留", 8, 54, 192, &muyu_font_16, MUTED);
    label(s_modal, "OK 确定", 8, 88, 192, &muyu_font_16, ACCENT);
    label(s_modal, "上/下 取消", 8, 116, 192, &muyu_font_16, MUTED);
    lv_obj_add_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
    lv_screen_load(s_screen);
}

void muyu_ui_update(const muyu_model_t *model, const muyu_status_t *status) {
    char text[64];
    snprintf(text, sizeof(text), "%" PRIu32, model->session);
    set_text(s_count, text);
    snprintf(text, sizeof(text), "累计 %" PRIu32, model->total);
    set_text(s_total, text);
    if (status->battery_percent < 0) snprintf(text, sizeof(text), "--%%");
    else snprintf(text, sizeof(text), "%d%%", status->battery_percent);
    set_text(s_battery, text);
    snprintf(text, sizeof(text), "%s %uBPM", model->auto_enabled ? "自动" : "手动",
             (unsigned)muyu_model_bpm(model));
    set_text(s_mode, text);
    if (muyu_model_volume_percent(model) == 0) snprintf(text, sizeof(text), "静音");
    else snprintf(text, sizeof(text), "声音 %u%%", (unsigned)muyu_model_volume_percent(model));
    set_text(s_sound, text);
    set_text(s_saved, status->saved ? "已保存" : "未保存");
    const char *warning = !status->storage_ok ? "存储不可用" :
                          !status->buttons_ok ? "按键不可用" :
                          !status->audio_ok ? "声音不可用" : "";
    if (warning[0] && status->storage_ok) {
        snprintf(text, sizeof(text), "%s %s", warning, status->saved ? "已保存" : "未保存");
        set_text(s_warning, text);
    } else set_text(s_warning, warning);
    lv_obj_set_style_text_opa(s_saved, warning[0] ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
    // Hide the underlying scene so no count/image fragments peek around the
    // modal's rounded corners, including the widest ten-digit counter.
    lv_obj_t *scene[] = { s_count, s_total, s_wood, s_plus };
    for (unsigned i = 0; i < sizeof(scene) / sizeof(scene[0]); ++i) {
        if (model->reset_confirm) lv_obj_add_flag(scene[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(scene[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (model->reset_confirm) lv_obj_remove_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_modal, LV_OBJ_FLAG_HIDDEN);
}

void muyu_ui_strike(uint64_t now_ms) {
    s_struck_at = now_ms;
    s_animating = true;
}

void muyu_ui_animate(uint64_t now_ms) {
    if (!s_animating) return;
    uint64_t elapsed = now_ms - s_struck_at;
    if (elapsed >= 420) {
        lv_obj_set_y(s_wood, 130);
        lv_obj_set_style_text_opa(s_plus, LV_OPA_TRANSP, 0);
        s_animating = false;
        return;
    }
    lv_obj_set_y(s_wood, elapsed < 100 ? 134 : 130);
    lv_obj_set_y(s_plus, 151 - (int)(elapsed * 22 / 420));
    lv_obj_set_style_text_opa(s_plus, (lv_opa_t)(255 - elapsed * 255 / 420), 0);
}
