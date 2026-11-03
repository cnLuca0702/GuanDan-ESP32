#include "lvgl.h"
#if LVGL_VERSION_MAJOR != 9 || LVGL_VERSION_MINOR != 5 || LVGL_VERSION_PATCH != 0
#error "Muyu preview requires exactly LVGL 9.5.0"
#endif
#include "muyu_ui.h"
#include "bsp_display_rounding.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

LV_FONT_DECLARE(muyu_font_16);
LV_FONT_DECLARE(muyu_font_20);

static uint16_t frame[320][240];
static uint8_t draw_buffer[240 * 20 * 2];
static unsigned problems;

static void flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *pixels) {
    const uint16_t *source = (const uint16_t *)pixels;
    for (int y = area->y1; y <= area->y2; ++y) {
        for (int x = area->x1; x <= area->x2; ++x) {
            const uint16_t color = *source++;
            if (x >= 0 && x < 240 && y >= 0 && y < 320) {
                frame[y][x] = bsp_display_pixel_outside_rounded_rect(x, y, 240, 320, 30) ? 0 : color;
            }
        }
    }
    lv_display_flush_ready(display);
}

static uint32_t next_utf8(const unsigned char **text) {
    uint32_t cp = *(*text)++;
    if (cp < 128) return cp;
    unsigned more = cp < 224 ? 1 : cp < 240 ? 2 : 3;
    cp &= (1u << (6 - more)) - 1;
    while (more--) cp = (cp << 6) | (*(*text)++ & 63u);
    return cp;
}

static void check_labels(lv_obj_t *obj, const char *state) {
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_check_type(obj, &lv_label_class)) {
        const char *text = lv_label_get_text(obj);
        if (text[0] && lv_obj_get_style_text_opa(obj, LV_PART_MAIN) != LV_OPA_TRANSP) {
            lv_area_t box;
            lv_obj_get_coords(obj, &box);
            const lv_font_t *font = lv_obj_get_style_text_font(obj, LV_PART_MAIN);
            lv_point_t text_size;
            lv_text_get_size(&text_size, text, font,
                lv_obj_get_style_text_letter_space(obj, LV_PART_MAIN),
                lv_obj_get_style_text_line_space(obj, LV_PART_MAIN),
                LV_COORD_MAX, LV_TEXT_FLAG_NONE);
            const int width = lv_obj_get_content_width(obj);
            const int height = lv_obj_get_content_height(obj);
            printf("LABEL %s %d,%d %dx%d text=%dx%d %s\n", state, box.x1, box.y1, width, height, text_size.x, text_size.y, text);
            if (box.x1 < 0 || box.y1 < 0 || box.x2 >= 240 || box.y2 >= 320 || text_size.x > width || text_size.y > height) {
                fprintf(stderr, "CLIPPING %s: %s\n", state, text);
                ++problems;
            }
            lv_obj_t *parent = lv_obj_get_parent(obj);
            if (parent && parent != lv_screen_active()) {
                lv_area_t parent_box;
                lv_obj_get_coords(parent, &parent_box);
                if (box.x1 < parent_box.x1 || box.x2 > parent_box.x2 || box.y1 < parent_box.y1 || box.y2 > parent_box.y2) {
                    fprintf(stderr, "PARENT CLIPPING %s: %s\n", state, text);
                    ++problems;
                }
            }
            const unsigned char *cursor = (const unsigned char *)text;
            while (*cursor) {
                uint32_t cp = next_utf8(&cursor);
                if (cp < 32) continue;
                lv_font_glyph_dsc_t glyph = {0};
                if (!lv_font_get_glyph_dsc(font, &glyph, cp, 0) || glyph.is_placeholder) {
                    fprintf(stderr, "MISSING GLYPH %s U+%04" PRIX32 "\n", state, cp);
                    ++problems;
                }
            }
        }
    }
    for (unsigned i = 0; i < lv_obj_get_child_count(obj); ++i) check_labels(lv_obj_get_child(obj, i), state);
}

static void save_frame(const char *directory, const char *state) {
    lv_obj_update_layout(lv_screen_active());
    lv_refr_now(NULL);
    check_labels(lv_screen_active(), state);
    char filename[1024];
    snprintf(filename, sizeof(filename), "%s/%s.ppm", directory, state);
    FILE *file = fopen(filename, "wb");
    assert(file);
    fprintf(file, "P6\n240 320\n255\n");
    for (unsigned y = 0; y < 320; ++y) for (unsigned x = 0; x < 240; ++x) {
        const uint16_t value = frame[y][x];
        uint8_t rgb[3] = { (uint8_t)((value >> 11) * 255 / 31),
                          (uint8_t)(((value >> 5) & 63) * 255 / 63),
                          (uint8_t)((value & 31) * 255 / 31) };
        fwrite(rgb, 1, 3, file);
    }
    fclose(file);
    lv_mem_monitor_t monitor;
    lv_mem_monitor(&monitor);
    printf("HEAP %s total=%zu free=%zu largest=%zu max_used=%zu frag=%u%%\n", state,
        monitor.total_size, monitor.free_size, monitor.free_biggest_size, monitor.max_used, monitor.frag_pct);
}

int main(int argc, char **argv) {
    assert(argc == 3);
    lv_init();
    lv_display_t *display = lv_display_create(240, 320);
    assert(display);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, draw_buffer, NULL, sizeof(draw_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush_cb);
    muyu_ui_create();
    muyu_model_t model;
    muyu_model_init(&model, 1280, MUYU_DEFAULT_VOLUME_INDEX, MUYU_DEFAULT_TEMPO_INDEX, 0);
    muyu_status_t status = { .battery_percent = 86, .audio_ok = true, .storage_ok = true, .saved = true, .buttons_ok = true };
    muyu_ui_update(&model, &status);
    save_frame(argv[1], "idle");
    muyu_model_input(&model, MUYU_OK_PRESS, 1000);
    muyu_ui_update(&model, &status);
    muyu_ui_strike(1000);
    muyu_ui_animate(1040);
    save_frame(argv[1], "strike");
    muyu_ui_animate(1500);
    muyu_model_input(&model, MUYU_UP_SHORT, 1500);
    muyu_model_input(&model, MUYU_UP_LONG, 1500);
    muyu_model_input(&model, MUYU_UP_LONG, 1500);
    muyu_ui_update(&model, &status);
    save_frame(argv[1], "auto");
    muyu_model_input(&model, MUYU_DOWN_LONG, 2000);
    muyu_ui_update(&model, &status);
    save_frame(argv[1], "modal");
    muyu_model_input(&model, MUYU_UP_SHORT, 2100);
    model.total = UINT32_MAX;
    model.session = UINT32_MAX;
    status.battery_percent = 100;
    muyu_ui_update(&model, &status);
    save_frame(argv[1], "max-count");
    muyu_model_input(&model, MUYU_DOWN_LONG, 2300);
    muyu_ui_update(&model, &status);
    save_frame(argv[1], "max-count-modal");
    muyu_model_input(&model, MUYU_UP_SHORT, 2400);
    status.battery_percent = -1;
    status.storage_ok = false;
    status.saved = false;
    muyu_ui_update(&model, &status);
    save_frame(argv[1], "error");
    model.session = 3;
    model.total = 1283;
    model.volume_index = 0;
    status.storage_ok = true;
    status.saved = false;
    muyu_ui_update(&model, &status);
    save_frame(argv[1], "muted-unsaved");
    status.saved = true;
    status.audio_ok = false;
    muyu_ui_update(&model, &status);
    save_frame(argv[1], "audio-failure");
    status.buttons_ok = false;
    status.saved = false;
    muyu_ui_update(&model, &status);
    save_frame(argv[1], "button-failure");
    for (unsigned i = 0; i < 2000; ++i) {
        muyu_ui_strike(10000 + i * 500);
        muyu_ui_animate(10040 + i * 500);
        lv_refr_now(NULL);
        muyu_ui_animate(10450 + i * 500);
        lv_refr_now(NULL);
    }
    char inventory[4096];
    FILE *inventory_file = fopen(argv[2], "rb");
    assert(inventory_file);
    size_t inventory_size = fread(inventory, 1, sizeof(inventory) - 1, inventory_file);
    assert(feof(inventory_file));
    fclose(inventory_file);
    inventory[inventory_size] = '\0';
    for (unsigned size = 0; size < 2; ++size) {
        const lv_font_t *font = size == 0 ? &muyu_font_16 : &muyu_font_20;
        lv_font_glyph_dsc_t glyph = {0};
        assert(lv_font_get_glyph_dsc(font, &glyph, 0x6728, 0) && !glyph.is_placeholder);
        for (uint32_t cp = 32; cp < 127; ++cp) {
            memset(&glyph, 0, sizeof(glyph));
            assert(lv_font_get_glyph_dsc(font, &glyph, cp, 0) && !glyph.is_placeholder);
        }
        const unsigned char *cursor = (const unsigned char *)inventory;
        while (*cursor) {
            uint32_t cp = next_utf8(&cursor);
            if (cp < 32) continue;
            memset(&glyph, 0, sizeof(glyph));
            assert(lv_font_get_glyph_dsc(font, &glyph, cp, 0) && !glyph.is_placeholder);
        }
        memset(&glyph, 0, sizeof(glyph));
        bool found = lv_font_get_glyph_dsc(font, &glyph, 0x9F98, 0);
        assert(!found || glyph.is_placeholder);
        printf("FONT %u: full inventory and ASCII PASS; U+9F98 negative control PASS\n", size ? 20 : 16);
    }
    lv_mem_monitor_t monitor;
    lv_mem_monitor(&monitor);
    printf("HEAP after-2000-strikes total=%zu free=%zu largest=%zu max_used=%zu frag=%u%%\n",
        monitor.total_size, monitor.free_size, monitor.free_biggest_size, monitor.max_used, monitor.frag_pct);
    printf("LVGL real-render validation: %s (%u issues)\n", problems ? "FAIL" : "PASS", problems);
    return problems ? EXIT_FAILURE : EXIT_SUCCESS;
}
