# dual_task_demo: cooperative multi-task scheduler demo App.
# Shared boilerplate in ../sample_common.cmake.
include(${CMAKE_CURRENT_LIST_DIR}/../sample_common.cmake)

set(APP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/device_tree.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app_callbacks.c)

# Export to wasm build
set(WINK_APP_SOURCES ${APP_SOURCES} PARENT_SCOPE)

# host e2e target only defined off-wasm
if(NOT EMSCRIPTEN)
    # P2 (2026-07-06): dal linked as a static library instead of listing sources.
    add_executable(app_dual_task_demo_e2e
        ${APP_SOURCES}
        ${WINK_SAMPLE_RUNTIME_SOURCES}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../test/stubs/js_sim_host_stub.c
        ${CMAKE_CURRENT_SOURCE_DIR}/test_dual_task_demo_e2e.c
        $<TARGET_OBJECTS:pal_host>)

    target_compile_definitions(app_dual_task_demo_e2e PRIVATE SIMULATION=1)

    # dual_task_demo pulls js_sim_host_stub.c which needs targets/wasm headers
    # for the js_* extern signatures. Passed as an extra include dir.
    wink_sample_apply_common(app_dual_task_demo_e2e
        ${CMAKE_CURRENT_SOURCE_DIR}/../../targets/wasm)
    target_link_libraries(app_dual_task_demo_e2e PRIVATE dal)

    add_test(NAME app_dual_task_demo_e2e COMMAND app_dual_task_demo_e2e)
endif()
