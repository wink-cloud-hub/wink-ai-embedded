# OLED Dashboard sample App (Phase 2 acceptance: Button + LED + SSD1306 I2C
# protocol bypass). Shared boilerplate in ../sample_common.cmake.
include(${CMAKE_CURRENT_LIST_DIR}/../sample_common.cmake)

set(APP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/device_tree.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app_callbacks.c)

# Export to top-level wink_simulator (wasm). Contract: §6.1 App injection /
# 2026-07-01 wasm simulator repair plan Task 1.2.
set(WINK_APP_SOURCES ${APP_SOURCES} PARENT_SCOPE)

# host e2e target depends on pal_host / test_oled_dashboard_e2e.c (host-only);
# skip target definition on wasm — only export WINK_APP_SOURCES above.
if(EMSCRIPTEN)
    return()
endif()

# host e2e binary: runtime + trace + pal_host + app + sample-specific e2e
# P2 (2026-07-06): dal is linked as a static library so WINK_USE_XXX pruning
# options take effect; sources no longer listed explicitly.
add_executable(app_oled_dashboard_e2e
    ${APP_SOURCES}
    ${WINK_SAMPLE_RUNTIME_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_oled_dashboard_e2e.c
    $<TARGET_OBJECTS:pal_host>)
wink_sample_apply_common(app_oled_dashboard_e2e)
target_link_libraries(app_oled_dashboard_e2e PRIVATE dal)
add_test(NAME app_oled_dashboard_e2e COMMAND app_oled_dashboard_e2e)
