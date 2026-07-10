# avoidance_car sample App (default App when WINK_APP_DIR is unset).
# Shared include-dir + compile-option boilerplate lives in ../sample_common.cmake.
include(${CMAKE_CURRENT_LIST_DIR}/../sample_common.cmake)

set(APP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/device_tree.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app_callbacks.c)

# Export to top-level wink_simulator (wasm). Host branch also picks this up
# via add_subdirectory but the top-level host branch doesn't read it — no-op.
# Contract: §6.1 App injection / 2026-07-01 wasm simulator repair plan Task 1.2.
set(WINK_APP_SOURCES ${APP_SOURCES} PARENT_SCOPE)

# host e2e target pulls in pal_host / test_app_e2e.c (host-only); on wasm we
# only export WINK_APP_SOURCES above and skip target definitions.
if(EMSCRIPTEN)
    return()
endif()

# ── host e2e binary: runtime + trace + pal_host + app ───────────────────
# P2 (2026-07-06): dal linked as a static library instead of listing sources.
add_executable(app_avoidance_car_e2e
    ${APP_SOURCES}
    ${WINK_SAMPLE_RUNTIME_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../test/test_app_e2e.c
    $<TARGET_OBJECTS:pal_host>)
wink_sample_apply_common(app_avoidance_car_e2e)
wink_sample_link_common(app_avoidance_car_e2e)
target_link_libraries(app_avoidance_car_e2e PRIVATE dal)
add_test(NAME app_avoidance_car_e2e COMMAND app_avoidance_car_e2e)

# ── ADR-0008 device-tree override e2e (host in-memory storage) ─────────
# device_tree_apply_flash_config → pal_storage → registry → real DAL
# apply_override rewrites global fields; empty/corrupt silently degrades.
add_executable(test_avoidance_override
    ${CMAKE_CURRENT_SOURCE_DIR}/device_tree.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../test/test_avoidance_override.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../test/unity/unity.c
    $<TARGET_OBJECTS:pal_host>)
wink_sample_apply_common(test_avoidance_override ${CMAKE_CURRENT_SOURCE_DIR}/../../test/unity)
wink_sample_link_common(test_avoidance_override)
target_link_libraries(test_avoidance_override PRIVATE dal)
add_test(NAME test_avoidance_override COMMAND test_avoidance_override)
