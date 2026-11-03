#include "guandan_ui.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(guandan_font_14);
LV_FONT_DECLARE(guandan_font_18);
LV_FONT_DECLARE(guandan_font_84);

#define BACKGROUND 0x151C26
#define RED_CARD 0xBF3847
#define BLUE_CARD 0x285FA4
#define WHITE 0xFFFFFF
#define MUTED 0xB5BECA

static lv_obj_t *s_screen, *s_red_card, *s_blue_card;
static lv_obj_t *s_red_level, *s_blue_level, *s_battery, *s_status, *s_modal;

static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y,
                       int width, const lv_font_t *font, uint32_t color) {
    lv_obj_t *object = lv_label_create(parent);
    lv_obj_set_style_text_font(object, font, 0);
    lv_obj_set_style_text_color(object, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(object, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(object, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(object, width);
    lv_obj_set_pos(object, x, y);
    lv_label_set_text(object, text);
    return object;
}

static void set_text(lv_obj_t *object, const char *text) {
    if (strcmp(lv_label_get_text(object), text) != 0) lv_label_set_text(object, text);
}

static lv_obj_t *card(int y, uint32_t color, const char *team,
                      const char *control, lv_obj_t **level) {
    lv_obj_t *object = lv_obj_create(s_screen);
    lv_obj_remove_style_all(object);
    lv_obj_set_pos(object, 10, y);
    lv_obj_set_size(object, 220, 115);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(object, 16, 0);
    lv_obj_set_style_bg_color(object, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    label(object, team, 13, 7, 42, &guandan_font_18, WHITE);
    lv_obj_t *hint = label(object, control, 64, 10, 144, &guandan_font_14, WHITE);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_RIGHT, 0);
    *level = label(object, "2", 12, 32, 196, &guandan_font_84, WHITE);
    return object;
}

static void create_reset_modal(void) {
    // Keep the confirmation objects absent during normal score rendering. Large
    // rank glyphs need an A8 scratch buffer, so an idle hidden dialog wastes RAM.
    // Cards are hidden while confirming, so no underlying rank can peek around
    // the opaque rounded modal. Avoid a full-screen alpha compositing layer.
    s_modal = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_modal);
    lv_obj_set_pos(s_modal, 12, 53);
    lv_obj_set_size(s_modal, 216, 214);
    lv_obj_remove_flag(s_modal, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(s_modal, 18, 0);
    lv_obj_set_style_bg_color(s_modal, lv_color_hex(0xF3F5F8), 0);
    lv_obj_set_style_bg_opa(s_modal, LV_OPA_COVER, 0);
    label(s_modal, "双方回到2?", 8, 28, 200, &guandan_font_18, 0x202C3C);
    label(s_modal, "再按OK确认", 8, 78, 200, &guandan_font_18, BLUE_CARD);
    label(s_modal, "上/下取消", 8, 121, 200, &guandan_font_18, 0x516184);
    label(s_modal, "8秒后取消", 8, 167, 200, &guandan_font_14, 0x66768A);
}

void guandan_ui_create(void) {
    s_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_screen);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(BACKGROUND), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *title = label(s_screen, "掼蛋", 17, 11, 80, &guandan_font_18, WHITE);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_LEFT, 0);
    s_battery = label(s_screen, "--%", 166, 14, 56, &guandan_font_14, MUTED);
    s_red_card = card(37, RED_CARD, "红方", "上键 短按+ 长按-", &s_red_level);
    s_blue_card = card(160, BLUE_CARD, "蓝方", "下键 短按+ 长按-", &s_blue_level);
    s_status = label(s_screen, "已保存", 8, 278, 224, &guandan_font_14, MUTED);
    label(s_screen, "OK 重置", 40, 298, 160, &guandan_font_14, WHITE);

    s_modal = NULL;
    lv_screen_load(s_screen);
}

void guandan_ui_update(const guandan_model_t *model, const guandan_status_t *status) {
    char text[48];
    set_text(s_red_level, guandan_level_label(model->red));
    set_text(s_blue_level, guandan_level_label(model->blue));
    if (status->battery_percent < 0) snprintf(text, sizeof(text), "--%%");
    else snprintf(text, sizeof(text), "%d%%", status->battery_percent);
    set_text(s_battery, text);

    uint32_t status_color = MUTED;
    if (!status->storage_ok) {
        snprintf(text, sizeof(text), "存储不可用");
        status_color = 0xFFC0C6;
    } else if (!status->buttons_ok) {
        snprintf(text, sizeof(text), "按键不可用 %s", status->saved ? "已保存" : "未保存");
        status_color = 0xFFC0C6;
    } else {
        snprintf(text, sizeof(text), "%s", status->saved ? "已保存" : "未保存");
    }
    set_text(s_status, text);
    lv_obj_set_style_text_color(s_status, lv_color_hex(status_color), 0);

    if (model->reset_pending) {
        lv_obj_add_flag(s_red_card, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_blue_card, LV_OBJ_FLAG_HIDDEN);
        if (!s_modal) create_reset_modal();
    } else {
        lv_obj_remove_flag(s_red_card, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(s_blue_card, LV_OBJ_FLAG_HIDDEN);
        if (s_modal) {
            // No timer, callback, or other task holds these dialog objects.
            lv_obj_delete(s_modal);
            s_modal = NULL;
        }
    }
}
