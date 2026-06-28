# wink-micro-os host unit tests (PC gcc, no ESP-IDF, no Emscripten)
# 范式迁移自 chigo-micro/project/embedded/sim/test/CMakeLists.txt

set(UNITY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/unity)

set(HOST_PAL_OBJECT $<TARGET_OBJECTS:pal_host>)

# Unity 是 vendor 代码，豁免严格警告（vendor 的无害 warning 不应阻塞测试构建）
set_source_files_properties(${UNITY_DIR}/unity.c PROPERTIES COMPILE_FLAGS "-w")

# 通用 host 测试：仅链 pal(INTERFACE,仅头) + Unity。
# Plan 3 起会在此扩展链接 targets/host；Plan 4 扩展 add_wink_test_sim。
function(add_wink_test name src)
    add_executable(${name}
        ${src}
        ${UNITY_DIR}/unity.c
        ${ARGN})
    target_include_directories(${name} PRIVATE
        ${UNITY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../pal/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../trace/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/include)
    target_link_libraries(${name} PRIVATE pal)
    if(MSVC)
        # /W4: strict warnings; /WX: warnings as errors (0 warning = build fails if any warning
        # /wd4100: match GCC -Wno-unused-parameter (unused function parameters are intentional in HAL contracts)
        # /wd4210: MSVC-specific extension warning about file-scope function-static variables (harmless in C)
        target_compile_options(${name} PRIVATE /W4 /WX /wd4100 /wd4210)
    else()
        target_compile_options(${name} PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter
            -Wno-error=stack-usage=)  # P-stack：host 测试栈充裕，只警告不阻塞
    endif()
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_wink_test(test_smoke test_smoke.c)

# 链接 targets/host（完整 PAL）的测试
function(add_wink_host_test name src)
    add_executable(${name}
        ${src}
        ${UNITY_DIR}/unity.c
        ${HOST_PAL_OBJECT}
        ${ARGN})
    target_include_directories(${name} PRIVATE
        ${UNITY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/stubs
        ${CMAKE_CURRENT_SOURCE_DIR}/../pal/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../trace/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/include)
    target_link_libraries(${name} PRIVATE pal)
    if(MSVC)
        # /W4: strict warnings; /WX: warnings as errors (0 warning = build fails if any warning
        # /wd4100: match GCC -Wno-unused-parameter (unused function parameters are intentional in HAL contracts)
        # /wd4210: MSVC-specific extension warning about file-scope function-static variables (harmless in C)
        target_compile_options(${name} PRIVATE /W4 /WX /wd4100 /wd4210)
    else()
        target_compile_options(${name} PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter
            -Wno-error=stack-usage=)  # P-stack：host 测试栈充裕，只警告不阻塞
    endif()
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_wink_host_test(test_trace test_trace.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../trace/src/wink_trace.c)

add_wink_host_test(test_runtime test_runtime.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_runtime.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_actuator_registry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_soft_timer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../trace/src/wink_trace.c)

add_wink_host_test(test_host_pal test_host_pal.c)

# Phase 5 Task 5-1：执行器关断注册表直接单测（链 registry + wink_trace）
add_wink_host_test(test_actuator_registry test_actuator_registry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_actuator_registry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../trace/src/wink_trace.c)

# Phase 2 Task 2-3：host 资源占用治理直接单测（链 pal_host，含 pal_resource_host.c）
add_wink_host_test(test_pal_resource test_pal_resource.c)

# PAL 契约完整性编译探针（防 ESP32 跨 target 符号漂移门禁；评审 P0-1 对策）。
# 任何被 ESP32/wasm/host 引用却未在 PAL 头定义的符号 → 此处编译失败。
add_wink_host_test(test_pal_contract test_pal_contract.c)

# target 无关 PWM 定时器分配状态机单测（router 经 pal_host OBJECT lib 链接）
add_wink_host_test(test_pal_pwm_router test_pal_pwm_router.c)

# ADR-0008 设备树覆写 blob 解析器 + CRC32 单测（wink_dev_config.c 经 pal_host OBJECT 链接）
add_wink_host_test(test_dev_config test_dev_config.c)

# ADR-0008 PAL 存储抽象 host 单测（pal_storage_host.c 经 pal_host OBJECT 链接）
add_wink_host_test(test_pal_storage test_pal_storage.c)

# ADR-0009 Wave1 物理退化算法库单测（wink_sim_physical.c 现已并入 pal_host OBJECT）
add_wink_host_test(test_sim_physical test_sim_physical.c)

set(DAL_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/src/dal_ultrasonic.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/src/dal_servo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/src/dal_led.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/src/dal_button.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/src/dal_ssd1306.c)

add_wink_host_test(test_dal_servo test_dal_servo.c ${DAL_SRCS})

add_wink_host_test(test_dal_led test_dal_led.c ${DAL_SRCS})

add_wink_host_test(test_dal_button test_dal_button.c ${DAL_SRCS})

add_wink_host_test(test_dal_ssd1306 test_dal_ssd1306.c ${DAL_SRCS})

add_wink_host_test(test_dal_ultrasonic test_dal_ultrasonic.c ${DAL_SRCS})

# 仿真分支（-DSIMULATION）测试：验证 ADR-0003 决策2 两端同源在仿真侧确被走到。
function(add_wink_test_sim name src)
    add_executable(${name}
        ${src}
        ${UNITY_DIR}/unity.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stubs/js_sim_host_stub.c
        ${HOST_PAL_OBJECT}
        ${DAL_SRCS}
        ${ARGN})
    target_include_directories(${name} PRIVATE
        ${UNITY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/stubs
        ${CMAKE_CURRENT_SOURCE_DIR}/../pal/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../targets/wasm     # wasm_bridge.h（dal SIMULATION 分支引用）
        ${CMAKE_CURRENT_SOURCE_DIR}/../trace/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/include)
    target_link_libraries(${name} PRIVATE pal)
    target_compile_definitions(${name} PRIVATE SIMULATION=1)
    if(MSVC)
        # /W4: strict warnings; /WX: warnings as errors (0 warning = build fails if any warning
        # /wd4100: match GCC -Wno-unused-parameter (unused function parameters are intentional in HAL contracts)
        # /wd4210: MSVC-specific extension warning about file-scope function-static variables (harmless in C)
        target_compile_options(${name} PRIVATE /W4 /WX /wd4100 /wd4210)
    else()
        target_compile_options(${name} PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter
            -Wno-error=stack-usage=)  # P-stack：host 测试栈充裕，只警告不阻塞
    endif()
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_wink_test_sim(test_dal_ultrasonic_sim test_dal_ultrasonic_sim.c)

add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
