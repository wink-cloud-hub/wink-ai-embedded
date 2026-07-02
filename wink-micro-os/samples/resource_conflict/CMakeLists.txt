# samples/resource_conflict — 反例样本，验证 pal_resource 冲突治理生效。
#
# 本 sample 不使用 wink_runtime 或 wink_app 回调，只需最小依赖：
#   - DAL 层（led/servo/gps/eeprom）
#   - PAL 层（pal_host OBJECT，提供 pal_resource + pal_gpio/pal_pwm 桩）
# 故不引入 CMake 硬拦截 wasm target 的 EMSCRIPTEN 检查——不适用即静默不构建。

# wasm target 直接跳过：本 sample 只跑 host（反例演示不需要 wasm/ESP32 桥接）
if(EMSCRIPTEN)
    return()
endif()

# ESP32 target 也无需构建：反例样本设计上是 host-only 的接线契约演示
# （真机上"抢同 pin"会直接电气冲突，验证冲突治理在 host 层完成即可）
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

target_include_directories(sample_resource_conflict PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    # PAL 头
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pal/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pal/include/osal
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pal/include/hal
    ${CMAKE_CURRENT_SOURCE_DIR}/../../targets/common/include
    # DAL 头（本 sample 只用 4 个子命名空间）
    ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/include/output
    ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/include/actuator
    ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/include/communication
    ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/include/storage)

target_link_libraries(sample_resource_conflict PRIVATE pal)

if(MSVC)
    target_compile_options(sample_resource_conflict PRIVATE /W4 /WX /wd4100 /wd4210)
else()
    target_compile_options(sample_resource_conflict PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter)
endif()

# 加入 CTest：sample_resource_conflict 是"反例断言样本"，退出码 0 = 冲突治理生效
add_test(NAME sample_resource_conflict COMMAND sample_resource_conflict)
