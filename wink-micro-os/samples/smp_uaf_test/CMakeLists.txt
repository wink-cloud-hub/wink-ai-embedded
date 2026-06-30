# SMP UAF 验证测试
#
# 测试目的：验证 pal_irq_synchronize() 真正解决了 SMP 多核系统中的 UAF 问题。
#
# 真机运行方法：
#   1. cd esp32_firmware
#   2. 修改 main/app_sources.cmake 指向本目录
#   3. idf.py build flash monitor
#
# 验证步骤：
#   1. 先运行有 synchronize() 的版本（默认） → 应该全部通过
#   2. 注释掉 app_callbacks.c 中的 TEST_ENABLE_SYNCHRONIZE=true
#      → 几十轮内就会触发 UAF，证明 synchronize 是必要的

set(APP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/device_tree.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app_callbacks.c
    ${CMAKE_CURRENT_SOURCE_DIR}/smp_uaf_test.c)

# host 端到端测试（注意：host 是单线程，永远测不出 UAF！）
# 本测试的价值在 ESP32 真机 SMP 双核环境，host 测试仅验证代码结构正确。
add_executable(app_smp_uaf_test_e2e
    ${APP_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../trace/src/wink_trace.c
    ${CMAKE_CURRENT_SOURCE_DIR}/test_smp_uaf_e2e.c
    $<TARGET_OBJECTS:pal_host>)

target_include_directories(app_smp_uaf_test_e2e PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${WINK_CONFIG_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pal/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pal/include/osal
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pal/include/hal
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

# 确保 wink_config.h 生成后再编译
add_dependencies(app_smp_uaf_test_e2e generate_config)

if(MSVC)
    target_compile_options(app_smp_uaf_test_e2e PRIVATE /W4 /WX /wd4100 /wd4210)
else()
    target_compile_options(app_smp_uaf_test_e2e PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter)
endif()

add_test(NAME app_smp_uaf_test_e2e COMMAND app_smp_uaf_test_e2e)
