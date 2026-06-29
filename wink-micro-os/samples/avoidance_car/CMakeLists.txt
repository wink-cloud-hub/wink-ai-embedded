# avoidance_car 示例 App（注入点演示）。
# 默认 App（WINK_APP_DIR 未指定时）。
set(APP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/device_tree.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app_callbacks.c)

# host 端到端测试可执行：runtime + trace + dal + targets/host + app
add_executable(app_avoidance_car_e2e
    ${APP_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/src/wink_runtime.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/src/wink_actuator_registry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/src/wink_soft_timer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../trace/src/wink_trace.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/src/sensor/dal_ultrasonic.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/src/actuator/dal_servo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../test/test_app_e2e.c
    $<TARGET_OBJECTS:pal_host>)

target_include_directories(app_avoidance_car_e2e PRIVATE
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
    # /W4: strict warnings; /WX: warnings as errors
    # /wd4100: unused parameters (matches GCC -Wno-unused-parameter)
    # /wd4210: MSVC extension warning about file-scope function-static (harmless)
    target_compile_options(app_avoidance_car_e2e PRIVATE /W4 /WX /wd4100 /wd4210)
else()
    target_compile_options(app_avoidance_car_e2e PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter)
endif()
add_test(NAME app_avoidance_car_e2e COMMAND app_avoidance_car_e2e)

# ADR-0008 设备树覆写端到端（host 内存存储）：device_tree_apply_flash_config 经
# pal_storage → 注册表 → 真实 DAL apply_override 改写全局字段；空/损坏静默降级。
add_executable(test_avoidance_override
    ${CMAKE_CURRENT_SOURCE_DIR}/device_tree.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/src/sensor/dal_ultrasonic.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/src/actuator/dal_servo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../test/test_avoidance_override.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../test/unity/unity.c
    $<TARGET_OBJECTS:pal_host>)

target_include_directories(test_avoidance_override PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
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
    ${CMAKE_CURRENT_SOURCE_DIR}/../../test/unity)

target_link_libraries(test_avoidance_override PRIVATE pal)
if(MSVC)
    target_compile_options(test_avoidance_override PRIVATE /W4 /WX /wd4100 /wd4210)
else()
    target_compile_options(test_avoidance_override PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter)
endif()
add_test(NAME test_avoidance_override COMMAND test_avoidance_override)
