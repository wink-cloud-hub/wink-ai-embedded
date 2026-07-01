# DevKitC 冒烟测试示例（S1-S8 真机全链路，host e2e 验证代码结构）

# ── wasm target 硬拦截（2026-07-01 wasm simulator repair plan Task 1.3）──
# 本 sample 面向 ESP32 裸板真机验证：test_devkitc_smoke_e2e.c 引 host_test_ctrl.h
# （host-only），业务逻辑本身依赖 ESP32 GPIO 的中断行为。wasm target 无意义，
# 直接硬报错拒绝 configure，比 return() 静默跳过更能让调用方明确知道选错了 App。
if(EMSCRIPTEN)
    message(FATAL_ERROR
        "Sample 'devkitc_smoke' is a physical hardware smoke test and is not "
        "supported on the WebAssembly simulator target. Use "
        "-DWINK_APP_DIR=<...>/samples/avoidance_car (or oled_dashboard) for wasm.")
endif()

set(APP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/device_tree.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app_callbacks.c)

# host 端到端测试可执行：runtime + trace + dal_led + dal_button + targets/host + app
add_executable(app_devkitc_smoke_e2e
    ${APP_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/src/wink_runtime.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/src/wink_actuator_registry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/src/wink_soft_timer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../trace/src/wink_trace.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/src/output/dal_led.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/src/input/dal_button.c
    ${CMAKE_CURRENT_SOURCE_DIR}/test_devkitc_smoke_e2e.c
    $<TARGET_OBJECTS:pal_host>)

target_include_directories(app_devkitc_smoke_e2e PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${WINK_CONFIG_DIR}
    # Phase 1 目录重组：pal/include/ 根目录 + osal/ + hal/ 子目录均在搜索路径中
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pal/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pal/include/osal
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pal/include/hal
    # Phase 2：targets/common/include（wink_sim_physical.h）
    ${CMAKE_CURRENT_SOURCE_DIR}/../../targets/common/include
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
    target_compile_options(app_devkitc_smoke_e2e PRIVATE /W4 /WX /wd4100 /wd4210)
else()
    target_compile_options(app_devkitc_smoke_e2e PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter)
endif()

add_test(NAME app_devkitc_smoke_e2e COMMAND app_devkitc_smoke_e2e)
