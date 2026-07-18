# wink-micro-os 核心源文件列表（供各 target 共享引用）
# 避免 host/wasm/esp32 各 target 重复硬编码源文件路径
# 使用方式：在各 target CMakeLists.txt 中 include 本文件

# ── Runtime 源文件 ──────────────────────────────────────────────────────────
set(WINK_RUNTIME_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/runtime/src/wink_runtime.c
    ${CMAKE_CURRENT_LIST_DIR}/runtime/src/wink_runtime_tasks.c
    ${CMAKE_CURRENT_LIST_DIR}/runtime/src/wink_actuator_registry.c
    ${CMAKE_CURRENT_LIST_DIR}/runtime/src/wink_soft_timer.c
    ${CMAKE_CURRENT_LIST_DIR}/runtime/src/wink_event.c
    # ADR-0008 设备树覆写 blob 解析器 + CRC32：runtime 层通用工具（无硬件依赖），
    # 从 pal/src/ 迁出以修复层级反转。
    ${CMAKE_CURRENT_LIST_DIR}/runtime/src/wink_dev_config.c
)

# ── Trace 源文件 ──────────────────────────────────────────────────────────
set(WINK_TRACE_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/trace/src/wink_trace.c
)

# ── DAL 源文件 ──────────────────────────────────────────────────────────
# Phase 3 分类组织：sensor / actuator / output / input / display / communication / storage
set(WINK_DAL_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/dal/src/sensor/dal_ultrasonic.c
    ${CMAKE_CURRENT_LIST_DIR}/dal/src/actuator/dal_servo.c
    ${CMAKE_CURRENT_LIST_DIR}/dal/src/output/dal_led.c
    ${CMAKE_CURRENT_LIST_DIR}/dal/src/input/dal_button.c
    ${CMAKE_CURRENT_LIST_DIR}/dal/src/display/dal_ssd1306.c
    ${CMAKE_CURRENT_LIST_DIR}/dal/src/communication/dal_gps.c
    ${CMAKE_CURRENT_LIST_DIR}/dal/src/storage/dal_eeprom.c
)

# ── Selftest 源文件（可选：通过链接或配置排除；此处默认并入核心，由 app 决定是否调用） ──
set(WINK_SELFTEST_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/runtime/selftest/src/selftest_core.c
    ${CMAKE_CURRENT_LIST_DIR}/runtime/selftest/src/selftest_pwm_router.c
    ${CMAKE_CURRENT_LIST_DIR}/runtime/selftest/src/selftest_i2c_scan.c
    ${CMAKE_CURRENT_LIST_DIR}/runtime/selftest/src/selftest_smp_stress.c
    ${CMAKE_CURRENT_LIST_DIR}/runtime/selftest/src/selftest_gpio_isr.c
    ${CMAKE_CURRENT_LIST_DIR}/runtime/selftest/src/selftest_rmt_loopback.c
    # ADR-0023 Stage 2.4: bringup shadow-task helper (S10 ultrasonic echo sim),
    # migrated from samples/common/. Gated by #ifndef WINK_STRICT_NONBLOCKING
    # so it compiles out of strict non-blocking images.
    ${CMAKE_CURRENT_LIST_DIR}/runtime/selftest/src/wink_sim_ultrasonic_echo.c
)

# ── BAL (Business Abstraction Layer) 源文件 — ADR-0023 Stage 2 ────────────
# Domain-mirrored implementations under bal/src/<domain>/.
# When building via the top-level CMakeLists (host/wasm),
# these are compiled into the standalone `wink_bal` static library; for the
# ESP-IDF component build that pulls ${WINK_CORE_SOURCES} directly, listing
# them here guarantees the ESP32 component also compiles them. Keep in sync
# with bal/CMakeLists.txt target_sources(wink_bal ...).
set(WINK_BAL_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/bal/src/wink_bal_stub.c
    ${CMAKE_CURRENT_LIST_DIR}/bal/src/output/wink_led_blink.c
    ${CMAKE_CURRENT_LIST_DIR}/bal/src/input/wink_button_events.c
    ${CMAKE_CURRENT_LIST_DIR}/bal/src/input/wink_button_events_irq.c
    ${CMAKE_CURRENT_LIST_DIR}/bal/src/comm/wink_telemetry_default.c
    ${CMAKE_CURRENT_LIST_DIR}/bal/src/sensor/wink_ultrasonic_poll.c
    ${CMAKE_CURRENT_LIST_DIR}/bal/src/sensor/wink_ultrasonic_distance_events.c
    ${CMAKE_CURRENT_LIST_DIR}/bal/src/actuator/wink_servo_sweep.c
    ${CMAKE_CURRENT_LIST_DIR}/bal/src/math/wink_pid.c
    ${CMAKE_CURRENT_LIST_DIR}/bal/src/math/wink_diff_drive_kinematics.c
    ${CMAKE_CURRENT_LIST_DIR}/bal/src/control/wink_closed_loop_motor.c
    ${CMAKE_CURRENT_LIST_DIR}/bal/src/control/wink_chassis.c
)

# ── 核心包含目录 ──────────────────────────────────────────────────────────
# Phase 1 目录重组：pal/include/ 根目录 + osal/ + hal/ 子目录均在搜索路径中。
# 这样保持向后兼容：现有代码的 #include "pal_hal.h" / #include "pal_osal.h" 无需修改。
# DAL 头文件按分类组织在子目录中，源文件用 #include "dal_xxx.h" 直接引用（无需前缀）。
set(WINK_CORE_INCLUDE_DIRS
    ${CMAKE_CURRENT_LIST_DIR}/pal/include
    ${CMAKE_CURRENT_LIST_DIR}/pal/include/osal
    ${CMAKE_CURRENT_LIST_DIR}/pal/include/hal
    ${CMAKE_CURRENT_LIST_DIR}/dal/include
    ${CMAKE_CURRENT_LIST_DIR}/dal/include/input
    ${CMAKE_CURRENT_LIST_DIR}/dal/include/output
    ${CMAKE_CURRENT_LIST_DIR}/dal/include/actuator
    ${CMAKE_CURRENT_LIST_DIR}/dal/include/display
    ${CMAKE_CURRENT_LIST_DIR}/dal/include/sensor
    ${CMAKE_CURRENT_LIST_DIR}/dal/include/communication
    ${CMAKE_CURRENT_LIST_DIR}/dal/include/storage
    ${CMAKE_CURRENT_LIST_DIR}/runtime/include
    ${CMAKE_CURRENT_LIST_DIR}/runtime/selftest/src  # wink_selftest_internal.h + registry.def
    ${CMAKE_CURRENT_LIST_DIR}/trace/include
    # BAL (ADR-0023 Stage 2) — public headers; use domain-prefixed includes
    ${CMAKE_CURRENT_LIST_DIR}/bal/include
)

# ── 聚合所有核心源文件 ──────────────────────────────────────────────────────────
set(WINK_CORE_SOURCES
    ${WINK_RUNTIME_SOURCES}
    ${WINK_TRACE_SOURCES}
    ${WINK_DAL_SOURCES}
    ${WINK_SELFTEST_SOURCES}
    ${WINK_BAL_SOURCES}
)
