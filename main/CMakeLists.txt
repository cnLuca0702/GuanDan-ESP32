idf_component_register(
    SRCS "guandan_main.c" "guandan_model.c" "guandan_input.c" "guandan_ui.c"
         "../assets/fonts/guandan_font_14.c"
         "../assets/fonts/guandan_font_18.c"
         "../assets/fonts/guandan_font_84.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_timer nvs_flash
)
