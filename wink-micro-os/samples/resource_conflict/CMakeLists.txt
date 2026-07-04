# samples/resource_conflict — counter-example sample verifying pal_resource
# conflict wiring. Host-only assertion sample (no wink_runtime callbacks,
# no wasm/ESP32 relevance). Shared boilerplate in ../sample_common.cmake.
include(${CMAKE_CURRENT_LIST_DIR}/../sample_common.cmake)

# wasm target: skip silently (counter-example demo doesn't need wasm/ESP32 bridging)
if(EMSCRIPTEN)
    return()
endif()

# ESP32 target: also skipped — on real hardware two devices claiming the same
# pin would just electrically conflict; verifying conflict governance at the
# host layer is enough.
if(ESP_PLATFORM)
    return()
endif()

add_executable(sample_resource_conflict
    ${CMAKE_CURRENT_SOURCE_DIR}/app_main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/src/output/dal_led.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/src/actuator/dal_servo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/src/communication/dal_gps.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/src/storage/dal_eeprom.c
    $<TARGET_OBJECTS:pal_host>)

wink_sample_apply_common(sample_resource_conflict)
target_link_libraries(sample_resource_conflict PRIVATE pal)

# CTest: sample_resource_conflict is a counter-example assertion sample;
# exit code 0 == conflict governance active.
add_test(NAME sample_resource_conflict COMMAND sample_resource_conflict)
