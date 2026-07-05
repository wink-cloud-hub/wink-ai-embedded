# devkitc_smoke: ESP32 bare-metal S1-S8 full-chain smoke (host e2e verifies
# code structure; real signal is on hardware). Shared boilerplate in
# ../sample_common.cmake.
include(${CMAKE_CURRENT_LIST_DIR}/../sample_common.cmake)

# ── wasm target hard reject (2026-07-01 wasm simulator repair plan Task 1.3) ──
# test_devkitc_smoke_e2e.c pulls host_test_ctrl.h (host-only) and the business
# logic depends on ESP32 GPIO interrupt behavior. FATAL_ERROR beats silent
# return() — the caller learns they picked the wrong App.
if(EMSCRIPTEN)
    message(FATAL_ERROR
        "Sample 'devkitc_smoke' is a physical hardware smoke test and is not "
        "supported on the WebAssembly simulator target. Use "
        "-DWINK_APP_DIR=<...>/samples/avoidance_car (or oled_dashboard) for wasm.")
endif()

set(APP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/device_tree.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app_callbacks.c)

# host e2e binary: runtime + trace + dal_led + dal_button + pal_host + app
add_executable(app_devkitc_smoke_e2e
    ${APP_SOURCES}
    ${WINK_SAMPLE_RUNTIME_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/src/output/dal_led.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/src/input/dal_button.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/src/sensor/dal_ultrasonic.c
    ${CMAKE_CURRENT_SOURCE_DIR}/test_devkitc_smoke_e2e.c
    $<TARGET_OBJECTS:pal_host>)
wink_sample_apply_common(app_devkitc_smoke_e2e)
add_test(NAME app_devkitc_smoke_e2e COMMAND app_devkitc_smoke_e2e)
