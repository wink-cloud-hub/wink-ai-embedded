# Wink Micro-OS core source file manifest (shared across target builds)
# Prevents host/wasm/esp32 targets from repeating source file lists.
# Usage: include this file in target CMakeLists.txt.

# ── Runtime Sources ──────────────────────────────────────────────────────────
set(WINK_RUNTIME_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/runtime/src/wink_runtime.c
    ${CMAKE_CURRENT_LIST_DIR}/runtime/src/wink_runtime_tasks.c
    ${CMAKE_CURRENT_LIST_DIR}/runtime/src/wink_actuator_registry.c
    ${CMAKE_CURRENT_LIST_DIR}/runtime/src/wink_soft_timer.c
    ${CMAKE_CURRENT_LIST_DIR}/runtime/src/wink_event.c
    ${CMAKE_CURRENT_LIST_DIR}/runtime/src/wink_dev_config.c
)

# ── Trace Sources ──────────────────────────────────────────────────────────
set(WINK_TRACE_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/trace/src/wink_trace.c
)

# ── DAL Sources ──────────────────────────────────────────────────────────
# ADR-0039: DAL .c are NOT aggregated into WINK_CORE_SOURCES.
# Each target injects enabled drivers via wink_dal_add_enabled_sources()
# after wink_dal_apply_pruning() (see cmake/wink_dal_drivers.cmake).
set(WINK_DAL_SOURCES)

# ── Selftest Sources ────────────────────────────────────────────────────────
set(WINK_SELFTEST_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/runtime/selftest/src/selftest_core.c
    ${CMAKE_CURRENT_LIST_DIR}/runtime/selftest/src/selftest_pwm_router.c
    ${CMAKE_CURRENT_LIST_DIR}/runtime/selftest/src/selftest_i2c_scan.c
    ${CMAKE_CURRENT_LIST_DIR}/runtime/selftest/src/selftest_smp_stress.c
    ${CMAKE_CURRENT_LIST_DIR}/runtime/selftest/src/selftest_gpio_isr.c
    ${CMAKE_CURRENT_LIST_DIR}/runtime/selftest/src/selftest_rmt_loopback.c
    ${CMAKE_CURRENT_LIST_DIR}/runtime/selftest/src/wink_sim_ultrasonic_echo.c
)

# ── BAL (Business Abstraction Layer) Sources ────────────────────────────────
set(WINK_BAL_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/bal/src/wink_bal_stub.c
    ${CMAKE_CURRENT_LIST_DIR}/bal/src/output/wink_led_blink.c
    ${CMAKE_CURRENT_LIST_DIR}/bal/src/input/wink_button_events.c
    ${CMAKE_CURRENT_LIST_DIR}/bal/src/input/wink_button_events_irq.c
    ${CMAKE_CURRENT_LIST_DIR}/bal/src/comm/wink_telemetry_default.c
    ${CMAKE_CURRENT_LIST_DIR}/bal/src/sensor/wink_ultrasonic_poll.c
    ${CMAKE_CURRENT_LIST_DIR}/bal/src/sensor/wink_ultrasonic_distance_events.c
    ${CMAKE_CURRENT_LIST_DIR}/bal/src/actuator/wink_rc_servo_sweep.c
    ${CMAKE_CURRENT_LIST_DIR}/bal/src/math/wink_pid.c
    ${CMAKE_CURRENT_LIST_DIR}/bal/src/math/wink_diff_drive_kinematics.c
    ${CMAKE_CURRENT_LIST_DIR}/bal/src/control/wink_closed_loop_dc_motor.c
    ${CMAKE_CURRENT_LIST_DIR}/bal/src/control/wink_chassis.c
)

# ── Core Include Directories ────────────────────────────────────────────────
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
    ${CMAKE_CURRENT_LIST_DIR}/dal/include/comm
    ${CMAKE_CURRENT_LIST_DIR}/dal/include/storage
    ${CMAKE_CURRENT_LIST_DIR}/runtime/include
    ${CMAKE_CURRENT_LIST_DIR}/runtime/selftest/src
    ${CMAKE_CURRENT_LIST_DIR}/trace/include
    ${CMAKE_CURRENT_LIST_DIR}/bal/include
)

# ── Aggregate Core Sources ──────────────────────────────────────────────────
set(WINK_CORE_SOURCES
    ${WINK_RUNTIME_SOURCES}
    ${WINK_TRACE_SOURCES}
    ${WINK_DAL_SOURCES}
    ${WINK_SELFTEST_SOURCES}
    ${WINK_BAL_SOURCES}
)
