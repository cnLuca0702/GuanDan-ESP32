#include "lvgl.h"
#if LVGL_VERSION_MAJOR != 9 || LVGL_VERSION_MINOR != 5 || LVGL_VERSION_PATCH != 0
#error "Guandan preview requires exactly LVGL 9.5.0"
#endif
#include "guandan_ui.h"
#include "bsp_display_rounding.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

LV_FONT_DECLARE(guandan_font_14);
LV_FONT_DECLARE(guandan_font_18);
LV_FONT_DECLARE(guandan_font_84);

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

static void validate_fonts(const char *inventory_path) {
    char inventory[4096];
    FILE *file = fopen(inventory_path, "rb");
    assert(file);
    size_t length = fread(inventory, 1, sizeof(inventory) - 1, file);
    assert(feof(file));
    fclose(file);
    inventory[length] = '\0';
    const lv_font_t *fonts[] = { &guandan_font_14, &guandan_font_18, &guandan_font_84 };
    const unsigned sizes[] = {14, 18, 84};
    for (unsigned i = 0; i < 3; ++i) {
        const lv_font_t *font = fonts[i];
        if (i < 2) for (uint32_t cp = 32; cp < 127; ++cp) {
            lv_font_glyph_dsc_t glyph = {0};
            assert(lv_font_get_glyph_dsc(font, &glyph, cp, 0) && !glyph.is_placeholder);
        }
        const unsigned char *cursor = (const unsigned char *)(i < 2 ? inventory : "0123456789JQKA");
        while (*cursor) {
            uint32_t cp = next_utf8(&cursor);
            if (cp < 32) continue;
            lv_font_glyph_dsc_t glyph = {0};
            assert(lv_font_get_glyph_dsc(font, &glyph, cp, 0) && !glyph.is_placeholder);
        }
        lv_font_glyph_dsc_t absent = {0};
        bool found = lv_font_get_glyph_dsc(font, &absent, 0x9F98, 0);
        assert(!found || absent.is_placeholder);
        printf("FONT %u: required inventory PASS; U+9F98 negative control PASS\n", sizes[i]);
    }
}

int main(int argc, char **argv) {
    assert(argc == 3);
    lv_init();
    lv_display_t *display = lv_display_create(240, 320);
    assert(display);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, draw_buffer, NULL, sizeof(draw_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush_cb);
    guandan_ui_create();
    guandan_model_t model;
    guandan_model_init(&model, 0, 0);
    guandan_status_t status = { .battery_percent = 86, .storage_ok = true, .saved = true, .buttons_ok = true };
    guandan_ui_update(&model, &status);
    save_frame(argv[1], "initial");
    for (unsigned level = 0; level < GD_LEVEL_COUNT; ++level) {
        model.red = level;
        model.blue = GD_LEVEL_MAX - level;
        guandan_ui_update(&model, &status);
        char name[32];
        snprintf(name, sizeof(name), "level-%02u", level);
        save_frame(argv[1], name);
    }
    model.red = 12;
    model.blue = 14;
    guandan_ui_update(&model, &status);
    save_frame(argv[1], "a1-a3");
    model.red = GD_LEVEL_MAX;
    model.blue = GD_LEVEL_MAX;
    guandan_ui_update(&model, &status);
    save_frame(argv[1], "both-max");
    guandan_model_input(&model, GD_OK, 1000);
    guandan_ui_update(&model, &status);
    save_frame(argv[1], "reset-modal");
    // With both cards hidden, neither the top nor bottom edge can expose rank
    // fragments outside the opaque reset dialog.
    const uint16_t background = ((0x15 >> 3) << 11) | ((0x1c >> 2) << 5) | (0x26 >> 3);
    assert(frame[40][120] == background && frame[274][120] == background);
    guandan_model_tick(&model, 1000 + GD_RESET_TIMEOUT_MS);
    guandan_ui_update(&model, &status);
    save_frame(argv[1], "reset-timeout");
    guandan_model_input(&model, GD_OK, 10000);
    guandan_model_input(&model, GD_OK, 10200);
    status.saved = false;
    status.battery_percent = -1;
    guandan_ui_update(&model, &status);
    save_frame(argv[1], "reset-unsaved");
    status.storage_ok = false;
    guandan_ui_update(&model, &status);
    save_frame(argv[1], "storage-error");
    status.buttons_ok = false;
    guandan_ui_update(&model, &status);
    save_frame(argv[1], "both-errors");
    status.storage_ok = true;
    status.buttons_ok = false;
    guandan_ui_update(&model, &status);
    save_frame(argv[1], "button-error");
    lv_mem_monitor_t before;
    lv_mem_monitor(&before);
    for (unsigned i = 0; i < 2000; ++i) {
        model.red = i % GD_LEVEL_COUNT;
        model.blue = (i / GD_LEVEL_COUNT) % GD_LEVEL_COUNT;
        model.reset_pending = (i % 3) == 0;
        guandan_ui_update(&model, &status);
        lv_refr_now(NULL);
    }
    model.red = model.blue = 0;
    model.reset_pending = false;
    guandan_ui_update(&model, &status);
    lv_refr_now(NULL);
    validate_fonts(argv[2]);
    lv_mem_monitor_t after;
    lv_mem_monitor(&after);
    printf("HEAP after-2000-updates total=%zu free=%zu largest=%zu max_used=%zu frag=%u%%\n",
        after.total_size, after.free_size, after.free_biggest_size, after.max_used, after.frag_pct);
    if (after.free_size < before.free_size) {
        fprintf(stderr, "HEAP LOSS %zu bytes\n", before.free_size - after.free_size);
        ++problems;
    }
    printf("LVGL real-render validation: %s (%u issues)\n", problems ? "FAIL" : "PASS", problems);
    return problems ? EXIT_FAILURE : EXIT_SUCCESS;
}
