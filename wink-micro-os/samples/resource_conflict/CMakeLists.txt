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

# P2 (2026-07-06): link dal as a static library (which transitively pulls pal
# via target_link_libraries(dal PUBLIC pal)), so we don't need to list each
# dal/src/**/*.c explicitly or link pal separately.
add_executable(sample_resource_conflict
    ${CMAKE_CURRENT_SOURCE_DIR}/app_main.c
    $<TARGET_OBJECTS:pal_host>)

wink_sample_apply_common(sample_resource_conflict)
target_link_libraries(sample_resource_conflict PRIVATE dal)

# CTest: sample_resource_conflict is a counter-example assertion sample;
# exit code 0 == conflict governance active.
add_test(NAME sample_resource_conflict COMMAND sample_resource_conflict)
