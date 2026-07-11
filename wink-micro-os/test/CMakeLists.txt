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
        # Phase 1 目录重组：pal/include/ 根目录 + osal/ + hal/ 子目录均在搜索路径中
        ${CMAKE_CURRENT_SOURCE_DIR}/../pal/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../pal/include/osal
        ${CMAKE_CURRENT_SOURCE_DIR}/../pal/include/hal
        ${CMAKE_CURRENT_SOURCE_DIR}/../pal/include/internal
        ${CMAKE_CURRENT_SOURCE_DIR}/../trace/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/include)
    # P2 (2026-07-06): link dal instead of pal alone so WINK_USE_XXX pruning
    # options propagate. dal PUBLIC-depends on pal (INTERFACE), so pal
    # headers come through transitively.
    target_link_libraries(${name} PRIVATE dal)
    if(MSVC)
        # /W4: strict warnings; /WX: warnings as errors (0 warning = build fails if any warning
        # /wd4100: match GCC -Wno-unused-parameter (unused function parameters are intentional in HAL contracts)
        # /wd4210: MSVC-specific extension warning about file-scope function-static variables (harmless in C)
        target_compile_options(${name} PRIVATE /W4 /WX /wd4100 /wd4210)
    else()
        target_compile_options(${name} PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter
            -Wno-missing-field-initializers
            -Wno-error=stack-usage=)  # P-stack：host 测试栈充裕，只警告不阻塞
    endif()
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_wink_test(test_smoke test_smoke.c)

# ADR-0017 P1-D2：编译期契约测试 —— 在 -DWINK_STRICT_NONBLOCKING=1 下编译 +
# 链接一个只引用非阻塞 API 的 TU。目标：
#   (a) 证明所有 WINK_BLOCKING 声明在严格模式下确实从头文件消失
#       （否则源码里"注释掉的负例"若被复活会以 undeclared identifier 报错）；
#   (b) 证明非阻塞对偶 API 仍然可见（否则某个 #ifndef 包围过宽会被本 TU 抓到）。
# 与 dal_ultrasonic 的 L1 nm 探针互补：nm 层看符号表，本测试看编译期头文件门控。
add_wink_test(test_pal_nonblocking_strict test_pal_nonblocking_strict.c)
target_compile_definitions(test_pal_nonblocking_strict PRIVATE WINK_STRICT_NONBLOCKING=1)
# 需要 dal_ 头文件的搜索路径。add_wink_test 默认没把 dal include 加进来（那是给
# pal-only 契约测试用的）。这里手工补齐：sensor/ storage/ communication/ 都要。
target_include_directories(test_pal_nonblocking_strict PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/sensor
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/storage
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/communication)

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
        # Phase 1 目录重组：pal/include/ 根目录 + osal/ + hal/ 子目录均在搜索路径中
        ${CMAKE_CURRENT_SOURCE_DIR}/../pal/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../pal/include/osal
        ${CMAKE_CURRENT_SOURCE_DIR}/../pal/include/hal
        # Phase 2：targets/common/include（wink_sim_physical.h）
        ${CMAKE_CURRENT_SOURCE_DIR}/../targets/common/include
        # Fix: pal_wasm_internal.h is referenced by wink_runtime.c under SIMULATION
        ${CMAKE_CURRENT_SOURCE_DIR}/../targets/wasm
        ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/input
        ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/output
        ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/actuator
        ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/display
        ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/sensor
        ${CMAKE_CURRENT_SOURCE_DIR}/../trace/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/include)
    # P2 (2026-07-06): link dal instead of pal alone (pal comes via dal PUBLIC).
    target_link_libraries(${name} PRIVATE dal)
    if(MSVC)
        # /W4: strict warnings; /WX: warnings as errors (0 warning = build fails if any warning
        # /wd4100: match GCC -Wno-unused-parameter (unused function parameters are intentional in HAL contracts)
        # /wd4210: MSVC-specific extension warning about file-scope function-static variables (harmless in C)
        target_compile_options(${name} PRIVATE /W4 /WX /wd4100 /wd4210)
    else()
        target_compile_options(${name} PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter
            -Wno-missing-field-initializers
            -Wno-error=stack-usage=)  # P-stack：host 测试栈充裕，只警告不阻塞
    endif()
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_wink_host_test(test_trace test_trace.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../trace/src/wink_trace.c)

# ADR-0016 / Task D-2：wink_trace_fault (task) 与 wink_trace_fault_from_isr (ISR)
# 双入口在同一 code 序列下的等价性硬门槛。用 pal_os_set_sim_isr_context() sim-hook
# 模拟 ISR 边界。
add_wink_host_test(test_wink_trace_isr_equivalence test_wink_trace_isr_equivalence.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../trace/src/wink_trace.c)

add_wink_host_test(test_runtime test_runtime.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_runtime.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_runtime_tasks.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_actuator_registry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_soft_timer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_event.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../trace/src/wink_trace.c)

# ADR-0023 Stage 1.4a / 1.4b-iii: WINK_PERIODIC_INVALID sentinel,
# wink_periodic_active_count(), wink_periodic_change_period()
# (LIGHT + MAY_BLOCK + self-set_period re-entrancy).
add_wink_host_test(test_periodic_basics test_periodic_basics.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_runtime_tasks.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_soft_timer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../trace/src/wink_trace.c)

# ADR-0023 Task 1.5 / ADR-0025 §5: LIGHT dispatch in-flag + WCET enforcement.
add_wink_host_test(test_light_dispatch_flag test_light_dispatch_flag.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_soft_timer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../trace/src/wink_trace.c)

add_wink_host_test(test_host_pal test_host_pal.c)

add_wink_host_test(test_wink_event test_wink_event.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_event.c)

# Target-independent cooperative scheduler algorithm test
add_wink_test(test_sim_scheduler test_sim_scheduler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../targets/common/src/wink_sim_scheduler.c)
target_include_directories(test_sim_scheduler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../targets/common/include)

# Phase 5 Task 5-1：执行器关断注册表直接单测（链 registry + wink_trace）
add_wink_host_test(test_actuator_registry test_actuator_registry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_actuator_registry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../trace/src/wink_trace.c)

# Phase 2 Task 2-3：host 资源占用治理直接单测（链 pal_host，含 pal_resource_host.c）
add_wink_host_test(test_pal_resource test_pal_resource.c)
add_wink_host_test(test_sim_scheduler_e2e test_sim_scheduler_e2e.c)
add_wink_host_test(test_sim_scheduler_zombie_gc test_sim_scheduler_zombie_gc.c)

# fixup 计划 F2：WCET 8002 fault 门禁（C2 修复的门禁测试）。
# 用物理墙钟（host_wall_clock.h）精准忙等 15ms 触发 5ms 阈值；
# 同时验证 pal_os_busy_wait_us 不误报 + App on_fault 被调（红线 16 / R2）。
add_wink_host_test(test_sim_scheduler_wcet_fault test_sim_scheduler_wcet_fault.c)

# fixup 计划 F5 Step 1：本 wave RR 语义边界锁定（R4）。
# Task 7 引入 PRNG 交错时此测试的 Case 2 需改为 NOT_EQUAL 反测。
add_wink_test(test_sim_scheduler_determinism test_sim_scheduler_determinism.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../targets/common/src/wink_sim_scheduler.c)
target_include_directories(test_sim_scheduler_determinism PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../targets/common/include)

# fixup 计划 F5 Step 2：真 host fiber 层栈下限 clamp 验证。
add_wink_host_test(test_sim_scheduler_stack_clamp test_sim_scheduler_stack_clamp.c)

# P0 E5-part2：wasm/host 协作调度下 mutex BLOCKED 路径 e2e 验证。
add_wink_host_test(test_sim_mutex_e2e test_sim_mutex_e2e.c)

# fixup 计划 F5 Step 3：avoidance_car 单任务业务字段 baseline 回归（R-002 缓解措施）。
# R5：baseline .h 缺失即编译失败（HAS_BASELINE_HEADER 由 CMake file(EXISTS) 门控）。
set(_BASELINE_H
    ${CMAKE_CURRENT_SOURCE_DIR}/baseline/avoidance_car_semantic_baseline.h)
if(EXISTS ${_BASELINE_H})
    # P2 (2026-07-06): dal linked via add_wink_host_test -> target_link_libraries(dal);
    # removed hard-coded dal/src/**/*.c paths.
    add_wink_host_test(test_single_task_semantic_regression
        test_single_task_semantic_regression.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../samples/avoidance_car/device_tree.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../samples/avoidance_car/app_callbacks.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_runtime.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_runtime_tasks.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_actuator_registry.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_soft_timer.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_event.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../trace/src/wink_trace.c)
    target_include_directories(test_single_task_semantic_regression PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../samples/avoidance_car
        ${WINK_CONFIG_DIR})
    target_compile_definitions(test_single_task_semantic_regression PRIVATE
        HAS_BASELINE_HEADER=1)
else()
    message(WARNING
        "F5 R5: avoidance_car_semantic_baseline.h missing at ${_BASELINE_H} — "
        "test_single_task_semantic_regression will not be built. Run once with "
        "human-review then git add the baseline .h before it re-enters CI.")
endif()

# PAL 统一中断抽象单元测试（v2.0 含中断锁、共享中断、RAII 临界区）
add_wink_host_test(test_pal_irq test_pal_irq.c)

# PAL 契约完整性编译探针（防 ESP32 跨 target 符号漂移门禁；评审 P0-1 对策）。
# 任何被 ESP32/wasm/host 引用却未在 PAL 头定义的符号 → 此处编译失败。
add_wink_host_test(test_pal_contract test_pal_contract.c)

# P1-L1 分级日志加固：LOG_E/W/I/D + LOG_TAG 自动注入 + ISR 安全分流 + SYS 回退
# test_pal_log_sys_fallback.c 是伙伴 TU，故意不定义 LOG_TAG，验证默认 SYS 标签。
add_wink_host_test(test_pal_log_hardening
    test_pal_log_hardening.c
    test_pal_log_sys_fallback.c)

# target 无关 PWM 定时器分配状态机单测（router 经 pal_host OBJECT lib 链接）
add_wink_host_test(test_pal_pwm_router test_pal_pwm_router.c)

# ADR-0008 设备树覆写 blob 解析器 + CRC32 单测（wink_dev_config.c 经 pal_host OBJECT 链接）
add_wink_host_test(test_dev_config test_dev_config.c)

# ADR-0008 PAL 存储抽象 host 单测（pal_storage_host.c 经 pal_host OBJECT 链接）
add_wink_host_test(test_pal_storage test_pal_storage.c)

# ADR-0009 Wave1 物理退化算法库单测（wink_sim_physical.c 现已并入 pal_host OBJECT）
add_wink_host_test(test_sim_physical test_sim_physical.c)

# P2 (2026-07-06): DAL sources are now compiled into the `dal` static library
# and linked via target_link_libraries(... dal) inside add_wink_host_test /
# add_wink_test_sim. Per-driver tests no longer need to list dal/src/**/*.c
# explicitly — they just compile their own test .c file and get the full DAL.
add_wink_host_test(test_dal_servo test_dal_servo.c)

add_wink_host_test(test_dal_led test_dal_led.c)

add_wink_host_test(test_dal_button test_dal_button.c)

add_wink_host_test(test_dal_ssd1306 test_dal_ssd1306.c)

add_wink_host_test(test_dal_ultrasonic test_dal_ultrasonic.c)

# Stage 0 Task 0.7: DAL deinit contract tests for the two stubbed drivers
add_wink_host_test(test_dal_eeprom test_dal_eeprom.c)
target_include_directories(test_dal_eeprom PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/storage)

add_wink_host_test(test_dal_gps test_dal_gps.c)
target_include_directories(test_dal_gps PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/communication)

# Stage 0 Task 0.1: PAL I2C bus lifecycle API host test
add_wink_host_test(test_pal_i2c_bus test_pal_i2c_bus.c)

# ADR-0023 Task 2.2: BAL wink_button_helper.
# wink_bal PUBLIC-links wink_runtime (which contains wink_runtime_tasks.c
# and wink_soft_timer.c) and dal; transitive deps bring in pal + trace.
add_wink_host_test(test_button_helper test_button_helper.c)
target_link_libraries(test_button_helper PRIVATE wink_bal)
# add_wink_host_test's default include dirs do NOT include bal/include/input
# (where wink_button_helper.h lives). wink_bal PUBLIC-includes that path, but
# add_wink_host_test adds include dirs BEFORE target_link_libraries runs, so
# we add it explicitly here to be safe.
target_include_directories(test_button_helper PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../bal/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../bal/include/input)
target_compile_definitions(test_button_helper PRIVATE WINK_STRICT_NONBLOCKING=1)

# ADR-0023 Task 2.1: BAL wink_led_blink_helper.
# wink_bal PUBLIC-links wink_runtime (which contains wink_runtime_tasks.c and
# wink_soft_timer.c) and dal; transitive deps bring in pal + trace.
add_wink_host_test(test_blink_helper test_blink_helper.c)
target_link_libraries(test_blink_helper PRIVATE wink_bal)
# add_wink_host_test's default include dirs do NOT include bal/include/output
# (where wink_blink_helper.h lives). wink_bal PUBLIC-includes that path, but
# add_wink_host_test adds include dirs BEFORE target_link_libraries runs, so
# we add it explicitly here to be safe.
target_include_directories(test_blink_helper PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../bal/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../bal/include/output)
target_compile_definitions(test_blink_helper PRIVATE WINK_STRICT_NONBLOCKING=1)

# ADR-0023 Task 2.3: BAL wink_telemetry_helper.
add_wink_host_test(test_bal_telemetry test_bal_telemetry.c)
target_link_libraries(test_bal_telemetry PRIVATE wink_bal)
# add_wink_host_test's default include dirs do NOT include bal/include/comm;
# add it explicitly (mirrors test_button_helper / test_blink_helper pattern).
target_include_directories(test_bal_telemetry PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../bal/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../bal/include/comm)

# BAL sonar helper test.
add_wink_host_test(test_bal_sonar test_bal_sonar.c)
target_link_libraries(test_bal_sonar PRIVATE wink_bal)
target_include_directories(test_bal_sonar PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../bal/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../bal/include/sensor)

# BAL servo helper test.
add_wink_host_test(test_bal_servo test_bal_servo.c)
target_link_libraries(test_bal_servo PRIVATE wink_bal)
target_include_directories(test_bal_servo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../bal/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../bal/include/actuator)

# Track A M1 / Task A-4：DAL↔PAL 资源接线 e2e 单测（验证 7 个 DAL init 正确 wire pal_resource_claim）
# P2: dal is linked via add_wink_host_test; need the extra include dirs for
# gps/eeprom headers that live in subdirectories not on the default surface.
add_wink_host_test(test_pal_resource_wire test_pal_resource_wire.c)
target_include_directories(test_pal_resource_wire PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/communication
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/storage)

# 仿真分支（-DSIMULATION）测试：验证 ADR-0003 决策2 两端同源在仿真侧确被走到。
function(add_wink_test_sim name src)
    add_executable(${name}
        ${src}
        ${UNITY_DIR}/unity.c
        ${CMAKE_CURRENT_SOURCE_DIR}/stubs/js_sim_host_stub.c
        ${HOST_PAL_OBJECT}
        ${ARGN})
    target_include_directories(${name} PRIVATE
        ${UNITY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/stubs
        # Phase 1 目录重组：pal/include/ 根目录 + osal/ + hal/ 子目录均在搜索路径中
        ${CMAKE_CURRENT_SOURCE_DIR}/../pal/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../pal/include/osal
        ${CMAKE_CURRENT_SOURCE_DIR}/../pal/include/hal
        # Phase 2：targets/common/include（wink_sim_physical.h）
        ${CMAKE_CURRENT_SOURCE_DIR}/../targets/common/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/input
        ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/output
        ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/actuator
        ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/display
        ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/sensor
        ${CMAKE_CURRENT_SOURCE_DIR}/../targets/wasm     # wasm_bridge.h（dal SIMULATION 分支引用）
        ${CMAKE_CURRENT_SOURCE_DIR}/../trace/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/include)
    # P2 (2026-07-06): link dal instead of pal alone.
    target_link_libraries(${name} PRIVATE dal)
    target_compile_definitions(${name} PRIVATE SIMULATION=1)
    if(MSVC)
        # /W4: strict warnings; /WX: warnings as errors (0 warning = build fails if any warning
        # /wd4100: match GCC -Wno-unused-parameter (unused function parameters are intentional in HAL contracts)
        # /wd4210: MSVC-specific extension warning about file-scope function-static variables (harmless in C)
        target_compile_options(${name} PRIVATE /W4 /WX /wd4100 /wd4210)
    else()
        target_compile_options(${name} PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter
            -Wno-missing-field-initializers
            -Wno-error=stack-usage=)  # P-stack：host 测试栈充裕，只警告不阻塞
    endif()
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_wink_test_sim(test_dal_ultrasonic_sim test_dal_ultrasonic_sim.c)

# ADR-0009 Wave1 按键抖动端到端（dal_button × 物理退化；链 pal_host 含算法库 + 注入）
add_wink_test_sim(test_button_debounce_e2e test_button_debounce_e2e.c)

# WASM 虚拟外设仿真状态机与拦截逻辑测试
add_wink_host_test(test_wasm_devices_sim test_wasm_devices_sim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../targets/wasm/devices/wasm_sim_registry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../targets/wasm/devices/wasm_dev_ssd1306.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../targets/wasm/devices/wasm_dev_servo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../targets/wasm/devices/wasm_dev_ultrasonic.c)

# Wave 4 selftest 框架 + 内建 selftest 用例
add_wink_host_test(test_wink_selftest test_wink_selftest.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/selftest/src/selftest_core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/selftest/src/selftest_pwm_router.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/selftest/src/selftest_i2c_scan.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/selftest/src/selftest_smp_stress.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/selftest/src/selftest_gpio_isr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/selftest/src/selftest_rmt_loopback.c
    # ADR-0023 Stage 2.4: S10 bringup shadow-task helper, gates itself with
    # #ifndef WINK_STRICT_NONBLOCKING so it links out of strict-mirror builds.
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/selftest/src/wink_sim_ultrasonic_echo.c)
# selftest 源需要访问 wink_selftest_internal.h 和 registry.def
target_include_directories(test_wink_selftest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/selftest/src)

# P2 (2026-07-06): DAL pruning sanity — link sanity under default (all-ON)
# config. References every public non-blocking non-strict-gated API so that
# any driver accidentally dropped from libdal.a shows up at link time.
# This is NOT a Unity test (plain main()) — use add_executable directly
# instead of add_wink_host_test (which pulls in unity.c requiring setUp/tearDown).
add_executable(test_dal_pruning
    test_dal_pruning.c
    ${HOST_PAL_OBJECT})
target_include_directories(test_dal_pruning PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${CMAKE_CURRENT_SOURCE_DIR}/../pal/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../pal/include/osal
    ${CMAKE_CURRENT_SOURCE_DIR}/../pal/include/hal
    ${CMAKE_CURRENT_SOURCE_DIR}/../targets/common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../targets/wasm
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/input
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/output
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/actuator
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/display
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/sensor
    ${CMAKE_CURRENT_SOURCE_DIR}/../trace/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/include)
target_link_libraries(test_dal_pruning PRIVATE dal)
if(MSVC)
    target_compile_options(test_dal_pruning PRIVATE /W4 /WX /wd4100 /wd4210)
else()
    target_compile_options(test_dal_pruning PRIVATE -Wall -Wextra -Werror
        -Wno-unused-parameter -Wno-missing-field-initializers
        -Wno-error=stack-usage=)
endif()
add_test(NAME test_dal_pruning COMMAND test_dal_pruning)

# P2 (2026-07-06): compile-negative test — verify WINK_UNAVAILABLE_MSG stubs
# fire when a driver is forced OFF. Uses a CMake script test that invokes
# the compiler directly at ctest time (not part of the default build) so
# that turning WINK_USE_SERVO=OFF in this negative case does not affect the
# rest of the build.
add_test(
    NAME test_dal_pruning_unavailable
    COMMAND ${CMAKE_COMMAND}
        -D "GCC=${CMAKE_C_COMPILER}"
        -D "SRC_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
        -D "DAL_INC=${CMAKE_CURRENT_SOURCE_DIR}/../dal/include"
        -D "PAL_INC=${CMAKE_CURRENT_SOURCE_DIR}/../pal/include"
        -D "WINK_STATUS_H=${CMAKE_CURRENT_SOURCE_DIR}/../pal/include/wink_status.h"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/test_dal_pruning_neg.cmake
)

add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
