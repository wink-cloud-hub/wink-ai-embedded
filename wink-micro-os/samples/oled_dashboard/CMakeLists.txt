# OLED Dashboard 示例 App（Phase 2 验收：Button + LED + SSD1306 I2C 协议旁路）。
set(APP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/device_tree.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app_main.c)

# host 端到端测试可执行：runtime + trace + dal + targets/host + app + 专用 e2e
add_executable(app_oled_dashboard_e2e
    ${APP_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/src/wink_runtime.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/src/wink_actuator_registry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/src/wink_soft_timer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../trace/src/wink_trace.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/src/dal_button.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/src/dal_led.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/src/dal_ssd1306.c
    ${CMAKE_CURRENT_SOURCE_DIR}/test_oled_dashboard_e2e.c
    $<TARGET_OBJECTS:pal_host>)

target_include_directories(app_oled_dashboard_e2e PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${WINK_CONFIG_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pal/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../trace/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../test
    ${CMAKE_CURRENT_SOURCE_DIR}/../../test/stubs)

if(MSVC)
    target_compile_options(app_oled_dashboard_e2e PRIVATE /W4)
else()
    target_compile_options(app_oled_dashboard_e2e PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter)
endif()
add_test(NAME app_oled_dashboard_e2e COMMAND app_oled_dashboard_e2e)
