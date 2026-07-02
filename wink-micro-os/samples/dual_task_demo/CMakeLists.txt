set(APP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/device_tree.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app_callbacks.c)

# 供 wasm 构建导出
set(WINK_APP_SOURCES ${APP_SOURCES} PARENT_SCOPE)

# 仅在非 wasm 时添加 host e2e 测试目标
if(NOT EMSCRIPTEN)
    add_executable(app_dual_task_demo_e2e
        ${APP_SOURCES}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/src/wink_runtime.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/src/wink_actuator_registry.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/src/wink_soft_timer.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../trace/src/wink_trace.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/src/actuator/dal_servo.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/src/sensor/dal_ultrasonic.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../test/stubs/js_sim_host_stub.c
        ${CMAKE_CURRENT_SOURCE_DIR}/test_dual_task_demo_e2e.c
        $<TARGET_OBJECTS:pal_host>)

    target_compile_definitions(app_dual_task_demo_e2e PRIVATE SIMULATION=1)

    target_include_directories(app_dual_task_demo_e2e PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${WINK_CONFIG_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../pal/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../pal/include/osal
        ${CMAKE_CURRENT_SOURCE_DIR}/../../pal/include/hal
        ${CMAKE_CURRENT_SOURCE_DIR}/../../targets/common/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../targets/wasm
        ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/include/input
        ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/include/output
        ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/include/actuator
        ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/include/display
        ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/include/sensor
        ${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../trace/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../test
        ${CMAKE_CURRENT_SOURCE_DIR}/../../test/stubs)

    if(MSVC)
        target_compile_options(app_dual_task_demo_e2e PRIVATE /W4 /WX /wd4100 /wd4210)
    else()
        target_compile_options(app_dual_task_demo_e2e PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter)
    endif()

    add_test(NAME app_dual_task_demo_e2e COMMAND app_dual_task_demo_e2e)
endif()
