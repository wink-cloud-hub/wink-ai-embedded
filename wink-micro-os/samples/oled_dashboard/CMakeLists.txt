# OLED Dashboard 示例 App（Phase 2 验收：Button + LED + SSD1306 I2C 协议旁路）。
set(APP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/device_tree.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app_main.c)

# 导出给顶层 wink_simulator (wasm) 消费。契约见 §6.1 App 注入接口
# / 2026-07-01 wasm simulator repair plan Task 1.2。
set(WINK_APP_SOURCES ${APP_SOURCES} PARENT_SCOPE)

# host e2e target 依赖 pal_host / test_oled_dashboard_e2e.c 等 host-only 组件，
# 在 wasm build（EMSCRIPTEN）下跳过定义，仅保留上面的 WINK_APP_SOURCES 导出。
if(EMSCRIPTEN)
    return()
endif()

# host 端到端测试可执行：runtime + trace + dal + targets/host + app + 专用 e2e
add_executable(app_oled_dashboard_e2e
    ${APP_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/src/wink_runtime.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/src/wink_actuator_registry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/src/wink_soft_timer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../trace/src/wink_trace.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/src/input/dal_button.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/src/output/dal_led.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/src/display/dal_ssd1306.c
    ${CMAKE_CURRENT_SOURCE_DIR}/test_oled_dashboard_e2e.c
    $<TARGET_OBJECTS:pal_host>)

target_include_directories(app_oled_dashboard_e2e PRIVATE
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
    target_compile_options(app_oled_dashboard_e2e PRIVATE /W4 /WX /wd4100 /wd4210)
else()
    target_compile_options(app_oled_dashboard_e2e PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter)
endif()
add_test(NAME app_oled_dashboard_e2e COMMAND app_oled_dashboard_e2e)
