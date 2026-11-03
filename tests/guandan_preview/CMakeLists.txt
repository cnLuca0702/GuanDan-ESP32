cmake_minimum_required(VERSION 3.22)
project(guandan_preview C CXX)
set(CMAKE_C_STANDARD 11)
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
if(NOT LVGL_SOURCE_DIR)
    set(LVGL_SOURCE_DIR "${REPO_ROOT}/build/host/lvgl")
endif()
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_LIST_DIR}/lv_conf.h" CACHE STRING "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory("${LVGL_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/lvgl")
add_executable(guandan_preview main.c
    "${REPO_ROOT}/main/guandan_ui.c"
    "${REPO_ROOT}/main/guandan_model.c"
    "${REPO_ROOT}/assets/fonts/guandan_font_14.c"
    "${REPO_ROOT}/assets/fonts/guandan_font_18.c"
    "${REPO_ROOT}/assets/fonts/guandan_font_84.c"
    "${REPO_ROOT}/components/bsp/src/bsp_display_rounding.c")
target_include_directories(guandan_preview PRIVATE "${REPO_ROOT}/main" "${REPO_ROOT}/components/bsp/src")
target_link_libraries(guandan_preview PRIVATE lvgl m)
