cmake_minimum_required(VERSION 3.15)
# wink-micro-os host unit tests (PC gcc, no ESP-IDF, no Emscripten)

set(UNITY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/unity)

# Tests that exercise Wasm-target PAL channels (pal_wasm_ch*.c) require
# <emscripten.h> and Wasm-only KEEPALIVE symbols. They cannot be compiled by
# the host toolchain and are instead built/run via the emcc+Node harness under
# test/wasm/ (see pal_adc/add_wink_wasm_adc_test.cmake). Skip them on host.
find_program(EMCC_EXECUTABLE emcc)
find_program(NODE_EXECUTABLE node)
if(EMCC_EXECUTABLE AND NODE_EXECUTABLE)
    set(WINK_BUILD_WASM_TESTS TRUE)
else()
    set(WINK_BUILD_WASM_TESTS FALSE)
endif()

set(HOST_PAL_OBJECT $<TARGET_OBJECTS:pal_host>)

# Unity vendor compile flags
set_source_files_properties(${UNITY_DIR}/unity.c PROPERTIES COMPILE_FLAGS "-w")

# Function to register a plain host unit test
function(add_wink_test name src)
    add_executable(${name}
        ${src}
        ${UNITY_DIR}/unity.c
        ${ARGN})
    target_include_directories(${name} PRIVATE
        ${UNITY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/stubs
        ${CMAKE_CURRENT_SOURCE_DIR}/../pal/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../pal/include/osal
        ${CMAKE_CURRENT_SOURCE_DIR}/../pal/include/hal
        ${CMAKE_CURRENT_SOURCE_DIR}/../pal/include/internal
        ${CMAKE_CURRENT_SOURCE_DIR}/../trace/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/include)
    target_link_libraries(${name} PRIVATE dal)
    if(MSVC)
        target_compile_options(${name} PRIVATE /W4 /WX /wd4100 /wd4210)
    else()
        target_compile_options(${name} PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter
            -Wno-missing-field-initializers
            -Wno-error=stack-usage=)
    endif()
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_wink_test(test_smoke e2e/test_smoke.c)

# ADR-0017 P1-D2 strict non-blocking check
add_wink_test(test_pal_nonblocking_strict unit/pal/test_pal_nonblocking_strict.c)
target_compile_definitions(test_pal_nonblocking_strict PRIVATE WINK_STRICT_NONBLOCKING=1)
target_include_directories(test_pal_nonblocking_strict PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/sensor
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/storage
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/comm)

# Function to register a host test linking host PAL and stubs
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
    target_link_libraries(${name} PRIVATE dal)
    if(MSVC)
        target_compile_options(${name} PRIVATE /W4 /WX /wd4100 /wd4210)
    else()
        target_compile_options(${name} PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter
            -Wno-missing-field-initializers
            -Wno-error=stack-usage=)
    endif()
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_wink_host_test(test_trace unit/runtime/test_trace.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../trace/src/wink_trace.c)

add_wink_host_test(test_wink_trace_isr_equivalence unit/runtime/test_wink_trace_isr_equivalence.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../trace/src/wink_trace.c)

add_wink_host_test(test_runtime unit/runtime/test_runtime.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_runtime.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_runtime_tasks.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_actuator_registry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_soft_timer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_event.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../trace/src/wink_trace.c)

add_wink_host_test(test_periodic_basics unit/runtime/test_periodic_basics.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_runtime_tasks.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_soft_timer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../trace/src/wink_trace.c)

add_wink_host_test(test_light_dispatch_flag unit/runtime/test_light_dispatch_flag.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_soft_timer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../trace/src/wink_trace.c)

add_wink_host_test(test_host_pal unit/pal/test_host_pal.c)
add_wink_host_test(test_pal_adc unit/pal/test_pal_adc.c)

add_wink_host_test(test_wink_event unit/runtime/test_wink_event.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_event.c)

# Target-independent cooperative scheduler algorithm test
add_wink_test(test_sim_scheduler unit/sim/test_sim_scheduler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../targets/common/src/wink_sim_scheduler.c)
target_include_directories(test_sim_scheduler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../targets/common/include)

add_wink_host_test(test_actuator_registry unit/dal/test_actuator_registry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_actuator_registry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../trace/src/wink_trace.c)

add_wink_host_test(test_pal_resource unit/pal/test_pal_resource.c)
add_wink_host_test(test_sim_scheduler_e2e unit/sim/test_sim_scheduler_e2e.c)
add_wink_host_test(test_sim_scheduler_zombie_gc unit/sim/test_sim_scheduler_zombie_gc.c)
add_wink_host_test(test_sim_scheduler_headless_jump unit/sim/test_sim_scheduler_headless_jump.c)
add_wink_host_test(test_sim_scheduler_wcet_fault unit/sim/test_sim_scheduler_wcet_fault.c)

add_wink_test(test_sim_scheduler_determinism unit/sim/test_sim_scheduler_determinism.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../targets/common/src/wink_sim_scheduler.c)
target_include_directories(test_sim_scheduler_determinism PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../targets/common/include)

add_wink_host_test(test_sim_scheduler_stack_clamp unit/sim/test_sim_scheduler_stack_clamp.c)
add_wink_host_test(test_sim_mutex_e2e unit/sim/test_sim_mutex_e2e.c)

set(_BASELINE_H
    ${CMAKE_CURRENT_SOURCE_DIR}/baseline/avoidance_car_semantic_baseline.h)
if(EXISTS ${_BASELINE_H} AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../samples/avoidance_car)
    add_wink_host_test(test_single_task_semantic_regression
        unit/sim/test_single_task_semantic_regression.c
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
        "F5 R5: avoidance_car_semantic_baseline.h missing at ${_BASELINE_H} ??"
        "test_single_task_semantic_regression will not be built. Run once with "
        "human-review then git add the baseline .h before it re-enters CI.")
endif()

add_wink_host_test(test_pal_irq unit/pal/test_pal_irq.c)
add_wink_host_test(test_pal_contract unit/pal/test_pal_contract.c)

add_wink_host_test(test_pal_log_hardening
    unit/pal/test_pal_log_hardening.c
    unit/pal/test_pal_log_sys_fallback.c)

add_wink_host_test(test_pal_pwm_router unit/pal/test_pal_pwm_router.c)
add_wink_host_test(test_pal_pwm_config unit/pal/test_pal_pwm_config.c)
add_wink_host_test(test_dev_config unit/dal/test_dev_config.c)
add_wink_host_test(test_pal_storage unit/pal/test_pal_storage.c)
add_wink_host_test(test_sim_physical unit/sim/test_sim_physical.c)
add_wink_host_test(test_dal_rc_servo unit/dal/test_dal_rc_servo.c)
add_wink_host_test(test_dal_led unit/dal/test_dal_led.c)
add_wink_host_test(test_dal_ws2812 unit/dal/test_dal_ws2812.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/src/output/dal_ws2812.c)
target_include_directories(test_dal_ws2812 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/output)
add_wink_host_test(test_dal_relay unit/dal/test_dal_relay.c)
add_wink_host_test(test_dal_button unit/dal/test_dal_button.c)
add_wink_host_test(test_dal_mono_oled unit/dal/test_dal_mono_oled.c)
add_wink_host_test(test_dal_analog_knob unit/dal/test_dal_analog_knob.c)
# Wasm-target channel sim tests (pal_wasm_ch*.c) require <emscripten.h> and are
# built/run by the emcc+Node harness under test/wasm/ — NOT the host gcc build.
# (Wiring: follow-up, modeled on wasm/pal_adc/add_wink_wasm_adc_test.cmake.)
add_wink_host_test(test_dal_buzzer unit/dal/test_dal_buzzer.c)
target_include_directories(test_dal_analog_knob PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/input)

add_wink_host_test(test_dal_keypad unit/dal/test_dal_keypad.c)
target_include_directories(test_dal_keypad PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/input)

add_wink_host_test(test_dal_dc_motor unit/dal/test_dal_dc_motor.c)
target_include_directories(test_dal_dc_motor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/actuator)

add_wink_host_test(test_dal_encoder unit/dal/test_dal_encoder.c)
target_include_directories(test_dal_encoder PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/sensor)

add_wink_host_test(test_dal_load_cell unit/dal/test_dal_load_cell.c)
target_include_directories(test_dal_load_cell PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/sensor)

add_wink_host_test(test_dal_ntc unit/dal/test_dal_ntc.c)
target_include_directories(test_dal_ntc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/sensor)

add_wink_host_test(test_dal_eeprom unit/dal/test_dal_eeprom.c)
target_include_directories(test_dal_eeprom PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/storage)

add_wink_host_test(test_dal_gps unit/dal/test_dal_gps.c)
target_include_directories(test_dal_gps PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/comm)

# test_dal_uart_rx_sim — Wasm-target channel test (pal_wasm_ch2_uart.c), requires
# <emscripten.h>; built/run by the test/wasm/ emcc+Node harness, not host gcc.

add_wink_host_test(test_dal_abi_freeze unit/dal/test_dal_abi_freeze.c)

add_wink_host_test(test_pal_i2c_bus unit/pal/test_pal_i2c_bus.c)
add_wink_host_test(test_pal_spi unit/pal/test_pal_spi.c)
add_wink_host_test(test_pal_rmt unit/pal/test_pal_rmt.c)
add_wink_host_test(test_pal_pcnt unit/pal/test_pal_pcnt.c)
add_wink_host_test(test_pal_uart unit/pal/test_pal_uart.c)
add_wink_host_test(test_pal_time_safety unit/pal/test_pal_time_safety.c)
add_wink_host_test(test_pal_atomic unit/pal/test_pal_atomic.c)
add_wink_host_test(test_pal_deferred unit/pal/test_pal_deferred.c)
add_wink_host_test(test_pal_dma unit/pal/test_pal_dma.c)
add_wink_host_test(test_pal_hwtimer unit/pal/test_pal_hwtimer.c)
add_wink_host_test(test_pal_mcpwm unit/pal/test_pal_mcpwm.c)
add_wink_host_test(test_pal_adc_continuous unit/pal/test_pal_adc_continuous.c)
add_wink_host_test(test_pal_lockfree_pipeline unit/pal/test_pal_lockfree_pipeline.c)
add_wink_host_test(test_pal_wasm_completion
    unit/pal/test_pal_wasm_completion.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../targets/wasm/pal_wasm_completion.c
)
target_include_directories(test_pal_wasm_completion PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../targets/wasm)

add_executable(test_pal_wasm_drain
    unit/pal/test_pal_wasm_drain.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../targets/wasm/pal_wasm_completion.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../targets/wasm/pal_wasm_hwtimer.c
    ${UNITY_DIR}/unity.c
)
target_include_directories(test_pal_wasm_drain PRIVATE
    ${UNITY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${CMAKE_CURRENT_SOURCE_DIR}/../pal/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../pal/include/hal
    ${CMAKE_CURRENT_SOURCE_DIR}/../pal/include/osal
    ${CMAKE_CURRENT_SOURCE_DIR}/../targets/common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../targets/wasm
)
add_test(NAME test_pal_wasm_drain COMMAND test_pal_wasm_drain)

add_wink_host_test(test_button_events unit/bal/test_button_events.c)
target_link_libraries(test_button_events PRIVATE wink_bal)
target_compile_definitions(test_button_events PRIVATE WINK_STRICT_NONBLOCKING=1)

add_wink_host_test(test_ultrasonic_distance_events unit/bal/test_ultrasonic_distance_events.c)
target_link_libraries(test_ultrasonic_distance_events PRIVATE wink_bal)
target_compile_definitions(test_ultrasonic_distance_events PRIVATE WINK_STRICT_NONBLOCKING=1)

add_wink_host_test(test_button_events_irq_degrade unit/bal/test_button_events_irq_degrade.c)
target_link_libraries(test_button_events_irq_degrade PRIVATE wink_bal)

add_wink_host_test(test_button_events_irq_strict
    unit/bal/test_button_events_irq_degrade.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../bal/src/input/wink_button_events.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../bal/src/input/wink_button_events_irq.c)
target_link_libraries(test_button_events_irq_strict PRIVATE wink_bal)
target_compile_definitions(test_button_events_irq_strict PRIVATE WINK_BUTTON_IRQ_STRICT=1)

add_wink_host_test(test_led_blink unit/bal/test_led_blink.c)
target_link_libraries(test_led_blink PRIVATE wink_bal)
target_compile_definitions(test_led_blink PRIVATE WINK_STRICT_NONBLOCKING=1)

add_wink_host_test(test_bal_telemetry unit/bal/test_bal_telemetry.c)
target_link_libraries(test_bal_telemetry PRIVATE wink_bal)

add_wink_host_test(test_bal_ultrasonic_poll unit/bal/test_bal_ultrasonic_poll.c)
target_link_libraries(test_bal_ultrasonic_poll PRIVATE wink_bal)

add_wink_host_test(test_bal_rc_servo_sweep unit/bal/test_bal_rc_servo_sweep.c)
target_link_libraries(test_bal_rc_servo_sweep PRIVATE wink_bal)

add_wink_test(test_pid unit/bal/test_pid.c)
target_link_libraries(test_pid PRIVATE wink_bal)

add_wink_host_test(test_bal_closed_loop_dc_motor unit/bal/test_bal_closed_loop_dc_motor.c)
target_link_libraries(test_bal_closed_loop_dc_motor PRIVATE wink_bal)

add_wink_host_test(test_bal_chassis unit/bal/test_bal_chassis.c)
target_link_libraries(test_bal_chassis PRIVATE wink_bal)

add_wink_host_test(test_pal_resource_wire unit/pal/test_pal_resource_wire.c)
target_include_directories(test_pal_resource_wire PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/comm
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/storage)

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
        ${CMAKE_CURRENT_SOURCE_DIR}/../pal/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../pal/include/osal
        ${CMAKE_CURRENT_SOURCE_DIR}/../pal/include/hal
        ${CMAKE_CURRENT_SOURCE_DIR}/../targets/common/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/input
        ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/output
        ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/actuator
        ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/display
        ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/sensor
        ${CMAKE_CURRENT_SOURCE_DIR}/../targets/wasm
        ${CMAKE_CURRENT_SOURCE_DIR}/../trace/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/include)
    target_link_libraries(${name} PRIVATE dal)
    target_compile_definitions(${name} PRIVATE SIMULATION=1)
    if(MSVC)
        target_compile_options(${name} PRIVATE /W4 /WX /wd4100 /wd4210)
    else()
        target_compile_options(${name} PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter
            -Wno-missing-field-initializers
            -Wno-error=stack-usage=)
    endif()
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_wink_test_sim(test_dal_ultrasonic_sim unit/dal/test_dal_ultrasonic_sim.c)
add_wink_test_sim(test_button_debounce_e2e unit/bal/test_button_debounce_e2e.c)

# test_wasm_devices_sim — G3 ABI export compatibility test; its symbols
# (pal_wasm_sim_reset_all_devices / get_pwm_duty_percent / get_gpio_output)
# live in targets/wasm/*.c which require <emscripten.h>, so it is built/run by
# the test/wasm/ emcc+Node harness, NOT the native host gcc build.

add_wink_host_test(test_wink_selftest unit/runtime/test_wink_selftest.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/selftest/src/selftest_core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/selftest/src/selftest_pwm_router.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/selftest/src/selftest_i2c_scan.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/selftest/src/selftest_smp_stress.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/selftest/src/selftest_gpio_isr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/selftest/src/selftest_rmt_loopback.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/selftest/src/wink_sim_ultrasonic_echo.c)
target_include_directories(test_wink_selftest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/selftest/src)

add_executable(test_dal_pruning
    unit/dal/test_dal_pruning.c
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

add_test(
    NAME test_dal_pruning_unavailable
    COMMAND ${CMAKE_COMMAND}
        -D "GCC=${CMAKE_C_COMPILER}"
        -D "SRC_DIR=${CMAKE_CURRENT_SOURCE_DIR}/unit/dal"
        -D "DAL_INC=${CMAKE_CURRENT_SOURCE_DIR}/../dal/include"
        -D "PAL_INC=${CMAKE_CURRENT_SOURCE_DIR}/../pal/include"
        -D "WINK_STATUS_H=${CMAKE_CURRENT_SOURCE_DIR}/../pal/include/wink_status.h"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/unit/dal/test_dal_pruning_neg.cmake
)

if(TARGET wink_arduino_compat)
    add_wink_host_test(test_arduino_core_compat compat/test_arduino_core_compat.cpp compat/test_arduino_core_compat_runner.c)
    target_link_libraries(test_arduino_core_compat PRIVATE wink_arduino_compat)
endif()

# ── MCS-51/8051 zero-code sim interception (Axis B, ADR-0070) ────────────────
if(TARGET wink_mcs51_core)
    set(_MCS51_FW_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../frameworks/mcs51)
    set(_MCS51_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/mcs51_generated)

    # Cleanup pass: UNMODIFIED Keil .c sample -> .cpp copy in the build tree.
    # The source is never edited in place (Spike-S2 §6).
    function(mcs51_cleanup_sample sample_name)
        set(_cpp ${_MCS51_GEN_DIR}/${sample_name}.cpp)
        add_custom_command(
            OUTPUT ${_cpp}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${_MCS51_GEN_DIR}
            COMMAND ${Python3_EXECUTABLE}
                ${_MCS51_FW_DIR}/tools/mcs51_cleanup.py
                ${CMAKE_CURRENT_SOURCE_DIR}/../frameworks/mcs51/test/samples/${sample_name}.c
                ${_cpp}
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../frameworks/mcs51/test/samples/${sample_name}.c
                    ${_MCS51_FW_DIR}/tools/mcs51_cleanup.py
            COMMENT "mcs51 cleanup: ${sample_name}.c -> ${sample_name}.cpp"
            VERBATIM)
        set(_MCS51_${sample_name}_CPP ${_cpp} PARENT_SCOPE)
    endfunction()

    mcs51_cleanup_sample(blinky)
    mcs51_cleanup_sample(blinky_timer0)
    mcs51_cleanup_sample(uart_printf)
    mcs51_cleanup_sample(uart_echo)
    mcs51_cleanup_sample(gpio_in_out)
    mcs51_cleanup_sample(int0_button)
    mcs51_cleanup_sample(sfr_rmw_isolation_test)
    mcs51_cleanup_sample(adc0832_read)
    mcs51_cleanup_sample(cms8s_adc_test)
    mcs51_cleanup_sample(iron_ntc)
    mcs51_cleanup_sample(seg_display_scan)

    # ── M6: board-config codegen seam (wink-app.json -> mcs51_board_config.h) ──
    # Stage3 (CPL-09/CPL-21): the framework bridge no longer auto-binds the
    # codegen-described ADC0832. Boards/harnesses bind it in their own
    # post-init hook (adc0832_device_attach); the generated header's
    # directory must be on the BINDING TU's include path (test_mcs51_iron_ntc_e2e
    # adds it below). Stage6 S6-3 (CPL-15): the header is board/app scope ONLY —
    # it is not on any framework library include path. Gate on the generator
    # existing so plain-C/CI checkouts without the wink-tools source
    # configure cleanly and just skip the iron_ntc codegen test (mirrors
    # the tier-b fixture gate).
    set(_MCS51_BOARD_CONFIG_GENERATOR
        "${WINK_TOOLS_ROOT}/tools/codegen/generators/mcs51_board_config.py")
    set(_MCS51_IRON_NTC_APP
        "${CMAKE_CURRENT_SOURCE_DIR}/../frameworks/mcs51/test/apps/iron_ntc/wink-app.json")
    if(WINK_TOOLS_ROOT AND EXISTS "${_MCS51_BOARD_CONFIG_GENERATOR}"
            AND EXISTS "${_MCS51_IRON_NTC_APP}")
        set(_MCS51_BOARD_CONFIG_H "${_MCS51_GEN_DIR}/mcs51_board_config.h")
        set(_MCS51_BOARD_JSON
            "${WINK_TOOLS_ROOT}/tools/codegen/boards/mcs51/stc89c52_devboard.json")
        if(NOT EXISTS "${_MCS51_BOARD_JSON}")
            set(_MCS51_BOARD_JSON "${CMAKE_CURRENT_SOURCE_DIR}/../../wink-tools/tools/codegen/boards/mcs51/stc89c52_devboard.json")
        endif()
        if(NOT EXISTS "${_MCS51_BOARD_JSON}")
            set(_MCS51_BOARD_JSON "${WINK_TOOLS_ROOT}/tools/codegen/boards/mcs51/generic_mcs51_devboard.json")
        endif()
        if(NOT EXISTS "${_MCS51_BOARD_JSON}")
            set(_MCS51_BOARD_JSON "${CMAKE_CURRENT_SOURCE_DIR}/../../wink-tools/tools/codegen/boards/mcs51/generic_mcs51_devboard.json")
        endif()
        add_custom_command(
            OUTPUT ${_MCS51_BOARD_CONFIG_H}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${_MCS51_GEN_DIR}
            COMMAND ${Python3_EXECUTABLE} ${_MCS51_BOARD_CONFIG_GENERATOR}
                --input ${_MCS51_IRON_NTC_APP}
                --output ${_MCS51_BOARD_CONFIG_H}
            DEPENDS ${_MCS51_IRON_NTC_APP}
                    ${_MCS51_BOARD_CONFIG_GENERATOR}
                    "${WINK_TOOLS_ROOT}/tools/codegen/templates/mcs51_board_config.h.j2"
                    ${_MCS51_BOARD_JSON}
            COMMENT "mcs51 codegen: iron_ntc wink-app.json -> mcs51_board_config.h"
            VERBATIM)
        add_custom_target(generate_mcs51_board_config DEPENDS ${_MCS51_BOARD_CONFIG_H})
        set(_MCS51_HAS_BOARD_CONFIG TRUE)
    else()
        message(STATUS "mcs51 M6: mcs51_board_config generator or iron_ntc app "
            "not found — skipping codegen board-config + iron_ntc e2e")
        set(_MCS51_HAS_BOARD_CONFIG FALSE)
    endif()

    # ── Stage7 S7-1: chip registration is link-time self-registration ────────
    # tests link wink_mcs51_cms8s_register / the family packages below, whose
    # register TUs carry static initializers; no fixture include dir is needed
    # and the bridge compiles exactly as production does.

    # Runtime sources every mcs51 host test links (fiber scheduler path).
    set(_MCS51_RUNTIME_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_runtime.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_runtime_tasks.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_actuator_registry.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_soft_timer.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../runtime/src/wink_event.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../trace/src/wink_trace.c)

    # Common wiring for mcs51 host tests: fiber scheduler (SIMULATION), C++17
    # dialect for the cleaned/hand-written C++ TUs, mcs51 framework include.
    # Stage7 S7-1: the legacy wink_mcs51_compat alias is gone — tests name the
    # four split targets explicitly (core + both families + board device; the
    # link graph is the registration mechanism now, so the cms8s register
    # OBJECT must stay on every test link line).
    function(add_mcs51_host_test name)
        add_wink_host_test(${name} ${ARGN})
        target_link_libraries(${name} PRIVATE
            wink_mcs51_core wink_mcs51_cms8s wink_mcs51_at89
            wink_mcs51_adc0832 wink_runtime wink_mcs51_cms8s_register)
        # The Keil super-loop (wink_mcs51_user_main) never returns, so it MUST
        # run on the cooperative fiber scheduler (pal_sim_scheduler_run), not
        # the host direct-call tick path. SIMULATION selects that path in
        # wink_runtime.c (wasm defines it globally; host opts in per-target).
        target_compile_definitions(${name} PRIVATE SIMULATION=1 WINK_MCU_AT89C52=1)
        if(NOT MSVC)
            target_compile_options(${name} PRIVATE
                $<$<COMPILE_LANGUAGE:CXX>:-std=c++17 -Wno-write-strings
                    -fno-exceptions -fno-rtti -fno-threadsafe-statics>)
        else()
            target_compile_options(${name} PRIVATE
                # /wd4245: Keil C51 mixes int/unsigned freely (`P1 &= ~0x01`
                # passes a signed int ~-result to the unsigned proxy param);
                # WinkSfr/WinkSbit mask to bit/byte internally.
                $<$<COMPILE_LANGUAGE:CXX>:/std:c++17 /utf-8 /wd4101 /wd4245>)
        endif()
        target_include_directories(${name} PRIVATE ${_MCS51_FW_DIR}/include)
        # Stage7 S7-1: chip headers are no longer re-exported under core
        # include/ (forwarding shims deleted); tests include the real package
        # paths, mirroring the wasm harness include surface.
        target_include_directories(${name} PRIVATE
            ${_MCS51_FW_DIR}/chips/cms8s78xx/include
            ${_MCS51_FW_DIR}/chips/at89c52/include)
        # Stage4 S4-2 Step 5: test-only family harness (mcs51_test_harness.h).
        target_include_directories(${name} PRIVATE ${_MCS51_FW_DIR}/test)
        if(TARGET generate_config)
            add_dependencies(${name} generate_config)
        endif()
    endfunction()

    # M1: polling blinky (SFR proxies + cooperative yield, no ISR dispatch).
    add_mcs51_host_test(test_mcs51_blinky_host
        ../frameworks/mcs51/test/core/test_mcs51_blinky_host.c
        ${_MCS51_blinky_CPP}
        ${_MCS51_RUNTIME_SRCS})

    # M2: Timer0 mode-1 50 ms overflow drives the ISR via catch-up (D1~D3).
    add_mcs51_host_test(test_mcs51_timer0_host
        ../frameworks/mcs51/test/core/test_mcs51_timer0.c
        ${_MCS51_blinky_timer0_CPP}
        ${_MCS51_RUNTIME_SRCS})

    # M3: UART SBUF write emits to console + capture buffer; TI set synchronously.
    add_mcs51_host_test(test_mcs51_uart_host
        ../frameworks/mcs51/test/core/test_mcs51_uart.c
        ${_MCS51_uart_printf_CPP}
        ${_MCS51_RUNTIME_SRCS})

    # Stage 2 T2: RX injection -> ISR mailbox -> polled TX echo through the live
    # channel-2 route (host recording fallback asserts the echoed bytes).
    add_mcs51_host_test(test_mcs51_uart_echo
        ../frameworks/mcs51/test/core/test_mcs51_uart_echo_e2e.c
        ${_MCS51_uart_echo_CPP}
        ${_MCS51_RUNTIME_SRCS})

    # M3: GPIO in->out sync — P3.2 key (latch-injected) drives P1.0 LED across
    # three repeated runtime runs (released -> pressed -> released).
    add_mcs51_host_test(test_mcs51_gpio_host
        ../frameworks/mcs51/test/core/test_mcs51_gpio.c
        ${_MCS51_gpio_in_out_CPP}
        ${_MCS51_RUNTIME_SRCS})

    # Channel-1 external Read-Pin seam: the same gpio_in_out button->LED sample
    # but the button is driven as a real external driver via js_pal_gpio_read_state
    # (host fallback in mcs51_uni_bridge.cpp), not by writing the P3 latch.
    add_mcs51_host_test(test_mcs51_gpio_external
        ../frameworks/mcs51/test/core/test_mcs51_gpio_external_e2e.c
        ${_MCS51_gpio_in_out_CPP}
        ${_MCS51_RUNTIME_SRCS})

    # Stage 2 T3: /INT0 external-interrupt e2e — the interrupt-driven
    # button->LED sample toggles P1.0 from the vector-0 ISR; the main loop
    # never reads the button, so a toggle proves the ext-int model sampled the
    # external falling edge via channel-1 read and vectored (host + wasm/Node
    # share the driver).
    add_mcs51_host_test(test_mcs51_int0
        ../frameworks/mcs51/test/core/test_mcs51_int0_e2e.c
        ${_MCS51_int0_button_CPP}
        ${_MCS51_RUNTIME_SRCS})

    # Part 2: 8-digit 7-segment display dynamic scanning e2e test (P0=segments, P2=digits)
    add_mcs51_host_test(test_mcs51_seg_display_e2e
        ../frameworks/mcs51/test/core/test_mcs51_seg_display_e2e.c
        ${_MCS51_seg_display_scan_CPP}
        ${_MCS51_RUNTIME_SRCS})

    # M3 review: UART vector-4 EA+ES gated ISR dispatch + TI-not-auto-cleared
    # (host-only framework unit test; drives the model hook directly).
    add_mcs51_host_test(test_mcs51_uart_isr
        ../frameworks/mcs51/test/core/test_uart_isr_dispatch.cpp)

    # GAP-02: UART TX-link readiness gate — Release: predicate mask +
    # per-reason counters incl. health_pot-equivalent zero-regression e2e.
    add_mcs51_host_test(test_mcs51_uart_tx_ready
        ../frameworks/mcs51/test/cms8s78xx/test_mcs51_uart_tx_ready.cpp)

    # GAP-02 STRICT twin: same TU, STRICT-built compat lib for the abort
    # policy (death tests via child re-execution). Cannot reuse
    # add_mcs51_host_test: it links the release compat lib, which would
    # duplicate every model symbol. Body mirrors that function exactly with
    # the library swapped (WINK_MCS51_STRICT reaches the TU via the strict
    # lib's PUBLIC definition).
    add_wink_host_test(test_mcs51_uart_tx_ready_strict
        ../frameworks/mcs51/test/cms8s78xx/test_mcs51_uart_tx_ready.cpp)
    target_link_libraries(test_mcs51_uart_tx_ready_strict PRIVATE wink_mcs51_core_strict wink_mcs51_cms8s_strict wink_mcs51_at89 wink_mcs51_adc0832 wink_runtime
        wink_mcs51_cms8s_register)
    target_compile_definitions(test_mcs51_uart_tx_ready_strict PRIVATE SIMULATION=1 WINK_MCU_AT89C52=1)
    if(NOT MSVC)
        target_compile_options(test_mcs51_uart_tx_ready_strict PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:-std=c++17 -Wno-write-strings
                -fno-exceptions -fno-rtti -fno-threadsafe-statics>)
    else()
        target_compile_options(test_mcs51_uart_tx_ready_strict PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:/std:c++17 /utf-8 /wd4101 /wd4245>)
    endif()
        target_include_directories(test_mcs51_uart_tx_ready_strict PRIVATE ${_MCS51_FW_DIR}/include)
        target_include_directories(test_mcs51_uart_tx_ready_strict PRIVATE
            ${_MCS51_FW_DIR}/chips/cms8s78xx/include
            ${_MCS51_FW_DIR}/chips/at89c52/include)
        target_include_directories(test_mcs51_uart_tx_ready_strict PRIVATE ${_MCS51_FW_DIR}/test)
    if(TARGET generate_config)
        add_dependencies(test_mcs51_uart_tx_ready_strict generate_config)
    endif()

    # GAP-23: unmodeled-XSFR tripwire — Release: counters + first-offender
    # addresses + health_pot zero-regression e2e.
    add_mcs51_host_test(test_mcs51_xsfr_tripwire
        ../frameworks/mcs51/test/cms8s78xx/test_mcs51_xsfr_tripwire.cpp)

    # GAP-09: family-gated XDATA aperture (CMS8S 1KB XRAM + CMS8S-only
    # XSFR window; classic 8052 configurable external aperture).
    add_mcs51_host_test(test_mcs51_xram_aperture
        ../frameworks/mcs51/test/core/test_mcs51_xram_aperture.cpp)

    # GAP-12: T2/3/4 period formulas follow Fsys.
    add_mcs51_host_test(test_mcs51_t234_fsys
        ../frameworks/mcs51/test/cms8s78xx/test_mcs51_t234_fsys.cpp)

    # GAP-12: duplicate ISR vector guard + P0..P3EXTIF write-0-to-clear.
    add_mcs51_host_test(test_mcs51_gap12
        ../frameworks/mcs51/test/core/test_mcs51_gap12.cpp)

    # Stage4 S4-1 (CPL-07/CPL-22): EXTIF W0C is chip silicon — chip suite.
    add_mcs51_host_test(test_mcs51_extif_w0c
        ../frameworks/mcs51/test/cms8s78xx/test_mcs51_extif_w0c.cpp)

    # GAP-23 STRICT twin: same TU, STRICT-built compat lib (death tests via
    # child re-execution). Mirrors the uart_tx_ready_strict block above.
    add_wink_host_test(test_mcs51_xsfr_tripwire_strict
        ../frameworks/mcs51/test/cms8s78xx/test_mcs51_xsfr_tripwire.cpp)
    target_link_libraries(test_mcs51_xsfr_tripwire_strict PRIVATE wink_mcs51_core_strict wink_mcs51_cms8s_strict wink_mcs51_at89 wink_mcs51_adc0832 wink_runtime
        wink_mcs51_cms8s_register)
    target_compile_definitions(test_mcs51_xsfr_tripwire_strict PRIVATE SIMULATION=1 WINK_MCU_AT89C52=1)
    if(NOT MSVC)
        target_compile_options(test_mcs51_xsfr_tripwire_strict PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:-std=c++17 -Wno-write-strings
                -fno-exceptions -fno-rtti -fno-threadsafe-statics>)
    else()
        target_compile_options(test_mcs51_xsfr_tripwire_strict PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:/std:c++17 /utf-8 /wd4101 /wd4245>)
    endif()
        target_include_directories(test_mcs51_xsfr_tripwire_strict PRIVATE ${_MCS51_FW_DIR}/include)
        target_include_directories(test_mcs51_xsfr_tripwire_strict PRIVATE
            ${_MCS51_FW_DIR}/chips/cms8s78xx/include
            ${_MCS51_FW_DIR}/chips/at89c52/include)
        target_include_directories(test_mcs51_xsfr_tripwire_strict PRIVATE ${_MCS51_FW_DIR}/test)
    if(TARGET generate_config)
        add_dependencies(test_mcs51_xsfr_tripwire_strict generate_config)
    endif()

    # Task R3 / ADR-0078: Two-phase IRQ, in-service priority masking & call stack isolation
    add_mcs51_host_test(test_mcs51_irq_arbitration
        ../frameworks/mcs51/test/cms8s78xx/test_mcs51_irq_arbitration.cpp)

    # GAP-01/04/13/22: per-family silicon reset seeds (CKCON/Fosc) and the
# vendor-verified IRQ source->vector/profile mapping table
    add_mcs51_host_test(test_mcs51_silicon_seeds
        ../frameworks/mcs51/test/core/test_mcs51_silicon_seeds.cpp)

    # Stage0 (PLAN-20260911-MCS51-S0): v2 schema freeze + caps_cache
    # snapshot + STRICT stable-number locks (zero behavior change).
    add_mcs51_host_test(test_mcs51_family_schema
        ../frameworks/mcs51/test/core/test_mcs51_family_schema.cpp)

    # Stage2 S2-0 (PLAN-20260911-MCS51-S2): context RAM budget lock
    # (sizeof print + no-growth ceiling; feeds stage2 §4 budget table).
    add_mcs51_host_test(test_mcs51_context_budget
        ../frameworks/mcs51/test/core/test_mcs51_context_budget.cpp)

    # Stage2 S2-1 (PLAN-20260911-MCS51-S2): soc_priv scheme-A isolation
    # (dual-context pool split, idx clamp, hooks isolation, classic null).
    add_mcs51_host_test(test_mcs51_soc_priv_isolation
        ../frameworks/mcs51/test/core/test_mcs51_soc_priv_isolation.cpp)

    # Stage5 S5-1 Step 2 (PLAN-20260911-MCS51-S5): family insulation —
    # classic extended vectors/raise blocked + XSFR window closed, dual
    # contexts of different families coexist without profile bleed.
    add_mcs51_host_test(test_mcs51_family_insulation
        ../frameworks/mcs51/test/core/test_mcs51_family_insulation.cpp)

    # GAP-23 freshness gate: the checked-in XSFR tripwire allowlist must
    # match what the audit script emits from the chips REG_CMS8S78XX.H
    # (catches a stale allowlist after a pin-mux register is added/removed).
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME test_mcs51_xsfr_allowlist_fresh
            COMMAND ${Python3_EXECUTABLE}
                    ${_MCS51_FW_DIR}/tools/mcs51_shim_audit.py
                    --check-xsfr-allowlist
                    ${_MCS51_FW_DIR}/chips/cms8s78xx/include/cms8s_xsfr_allowlist.h)

        # Stage6 S6-2 CI assessment: the Keil-dialect cleanup unit suite was
        # manual; it is hermetic (stdlib unittest + checked-in manifests) so
        # it moves into the host ctest set. Verdict-parity for the SDCC gate
        # stays manual (needs SDCC + the uncommitted docs/vendors tree) and
        # the headless evidence script stays Win-only/manual — both are
        # written exemptions recorded in stage6 §6.
        add_test(NAME test_mcs51_cleanup_unit
            COMMAND ${Python3_EXECUTABLE} -m unittest
                    discover -s ${_MCS51_FW_DIR}/test/core
                    -p test_mcs51_cleanup.py)

        # Stage6 review S6-H5: the manifest-driven layering/residue gate is
        # hermetic stdlib — run it in the host ctest set too (was manual).
        add_test(NAME test_mcs51_layering_gate
            COMMAND ${Python3_EXECUTABLE}
                    ${_MCS51_FW_DIR}/tools/lint/lint_mcs51_layering.py)
    endif()

    # Task R0 / ADR-0077: GPIO dual read path (Read-Pin vs Read-Latch) & arbitration
    add_mcs51_host_test(test_mcs51_gpio_dual_path
        ../frameworks/mcs51/test/core/test_mcs51_gpio_dual_path.cpp)

    # Task R6: PCON IDL/PD low power dual-mode scheduling
    add_mcs51_host_test(test_mcs51_low_power
        ../frameworks/mcs51/test/core/test_mcs51_low_power.cpp)

    # Task F1: Timed edge event injection queue
    add_mcs51_host_test(test_mcs51_edge_queue
        ../frameworks/mcs51/test/core/test_mcs51_edge_queue.cpp)

    # Task F2: TMOD C/T=1 external pulse counter modeling
    add_mcs51_host_test(test_mcs51_timer_ext_clk
        ../frameworks/mcs51/test/core/test_mcs51_timer_ext_clk.cpp)

    # Task F3: Dynamic microstep quantum calibration
    add_mcs51_host_test(test_mcs51_clock_quantum
        ../frameworks/mcs51/test/core/test_mcs51_clock_quantum.cpp)

    # Task F4: Channel 1b Soft PWM duty cycle measurement (< 2% error)
    add_mcs51_host_test(test_mcs51_soft_pwm
        ../frameworks/mcs51/test/core/test_mcs51_soft_pwm.cpp)

    # Stage 2 T2: UART RX model — pending FIFO drained at microstep points,
    # REN gate, RI latch + vector 4, no-FIFO drop on RI pending, TX/RX shadow
    # interleave (host-only framework unit test; drives the model directly).
    add_mcs51_host_test(test_mcs51_uart_rx
        ../frameworks/mcs51/test/core/test_uart_rx_model.cpp)

    # Stage 2 T3: INT0/INT1 external interrupt model — edge-mode falling latch
    # + vector 0/2 with hardware auto-clear, pending-through-disable, level-mode
    # per-slice re-request, 10 ms sample throttle, HiZ ignore, reset semantics
    # (host-only framework unit test; drives the model directly).
    add_mcs51_host_test(test_mcs51_extint
        ../frameworks/mcs51/test/cms8s78xx/test_extint_model.cpp)

    # CMS8S78xx port external interrupt model (P0EI..P3EI vectors 7..10)
    add_mcs51_host_test(test_mcs51_port_extint
        ../frameworks/mcs51/test/cms8s78xx/test_mcs51_port_extint.cpp)

    # M2: 100 ms virtual delay == 10 master ticks, 1:1 clock conservation.
    add_mcs51_host_test(test_mcs51_clock_mapping
        ../frameworks/mcs51/test/core/test_unisim_clock_mapping.cpp
        ../frameworks/mcs51/test/core/mcs51_clock_user.cpp
        ${_MCS51_RUNTIME_SRCS})

    # M2: 3-TU static-init ISR registration + execution-phase gate (D5).
    add_mcs51_host_test(test_mcs51_static_init
        ../frameworks/mcs51/test/core/test_static_init_safety.cpp
        ../frameworks/mcs51/test/core/mcs51_static_tu_a.cpp
        ../frameworks/mcs51/test/core/mcs51_static_tu_b.cpp
        ../frameworks/mcs51/test/core/mcs51_static_tu_c.cpp
        ${_MCS51_RUNTIME_SRCS})

    # M3: intrins _crol_/_cror_/_testbit_ shims, absacc XBYTE/XWORD linear
    # XDATA shadow (aperture + R-008 OOB semantics), and the WINK_MCS51_STRICT
    # dual-mode unsupported-feature latch (release warn-once path).
    add_mcs51_host_test(test_mcs51_shims
        ../frameworks/mcs51/test/core/test_mcs51_shims.cpp)

    # 51 family routing: mcs51_family_route.h routes to REG_CMS8S78XX.H /
    # REGX52.H (Stage3: slimmed from the wink_mcu.h facade, now up at
    # runtime/include/wink_mcu.h).
    add_mcs51_host_test(test_mcs51_wink_mcu
        ../frameworks/mcs51/test/cms8s78xx/test_mcs51_wink_mcu.cpp)

    # M4: data plane — RMW Read-Latch integrity (external-low pin never
    # written back into the latch, whole-port + sbit RMW).
    add_mcs51_host_test(test_mcs51_sfr_rmw_latch
        ../frameworks/mcs51/test/core/test_sfr_rmw_latch_integrity.cpp)

    # M4: diff = old^val edge dispatch accuracy (Zero False-Trigger across
    # DI/CLK/CS-style pins) + linear pin map (port<<3)|bit via host sink log.
    add_mcs51_host_test(test_mcs51_sfr_edge
        ../frameworks/mcs51/test/core/test_sfr_edge_dispatch_accuracy.cpp)

    # M4: full operator algebra |= &= ^= += -= <<= >>=, pre/post ++/--, sbit
    # ops, copy assignment, absolute-form sbit, vendor non-GPIO SFR.
    add_mcs51_host_test(test_mcs51_sfr_operators
        ../frameworks/mcs51/test/core/test_sfr_operators_coverage.cpp)

    # M4: ADC0832 3-wire DIO instant state machine — sbit vs whole-port RMW
    # drive parity, CH0/CH1 mux, null-bit alignment, bus-release absorption.
    add_mcs51_host_test(test_mcs51_adc0832
        ../frameworks/mcs51/test/core/test_adc0832_dio_shared.cpp)

    # Stage3-H7 decision-B contract: adc0832_device_attach() with explicit
    # ch0/ch1_net_id (custom 40/41 end-to-end, unbound 32/33 fallback,
    # zero-net clamp). The default-key path stays with the suites above.
    add_mcs51_host_test(test_mcs51_adc0832_attach
        ../frameworks/mcs51/test/core/test_adc0832_attach_net_id.cpp)

    # M4: Zero False-Trigger acceptance (data-plane SSOT §4.3) — unmodified
    # Keil sample exercises the four RMW isolation vectors; the C driver binds
    # per-pin edge traps + a Read-Pin button trap and asserts XDATA snapshots.
    add_mcs51_host_test(test_mcs51_rmw_isolation
        ../frameworks/mcs51/test/core/test_mcs51_sfr_rmw_isolation.c
        ${_MCS51_sfr_rmw_isolation_test_CPP}
        ${_MCS51_RUNTIME_SRCS})

    # M4: ADC0832 3-wire DIO end-to-end through an unmodified Keil bit-bang
    # sample (CH0/CH1 read via instant trap FSM + channel-3 injection rail).
    add_mcs51_host_test(test_mcs51_adc0832_e2e
        ../frameworks/mcs51/test/core/test_mcs51_adc0832_e2e.c
        ${_MCS51_adc0832_read_CPP}
        ${_MCS51_RUNTIME_SRCS})

    # M5: CMS8S78xx on-chip ADC — 0-cycle instant model: ADGO self-clear,
    # right/left ADRESH/ADRESL packing, ADCIE/EA vector-19 gating, ADEN gate,
    # AN25/AN63 channels, XSFR proxy window + OOB trap.
    add_mcs51_host_test(test_mcs51_cms8s_adc
        ../frameworks/mcs51/test/cms8s78xx/test_cms8s_adc_instant.cpp)
    target_compile_definitions(test_mcs51_cms8s_adc PRIVATE WINK_MCU_CMS8S78XX=1)

    # M5: CMS8S78xx ADC end-to-end through an unmodified Keil polled sample
    # (real register map; AN0 right / AN1 left / AN25 right via 12-bit rail).
    add_mcs51_host_test(test_mcs51_cms8s_adc_e2e
        ../frameworks/mcs51/test/cms8s78xx/test_mcs51_cms8s_adc_e2e.c
        ${_MCS51_cms8s_adc_test_CPP}
        ${_MCS51_RUNTIME_SRCS})
    target_compile_definitions(test_mcs51_cms8s_adc_e2e PRIVATE WINK_MCU_CMS8S78XX=1)

    # CMS8S78xx on-chip Buzzer peripheral model test
    add_mcs51_host_test(test_mcs51_cms8s_buzzer
        ../frameworks/mcs51/test/cms8s78xx/test_cms8s_buzzer.cpp)
    target_compile_definitions(test_mcs51_cms8s_buzzer PRIVATE WINK_MCU_CMS8S78XX=1)

    # Stage 3 A-02: ADC reference chain — VSEL/Vrail scaling (Pull track),
    # LDO/mux readiness gates + counters, DIV-sensitive charge_us.
    add_mcs51_host_test(test_mcs51_adc_refchain
        ../frameworks/mcs51/test/cms8s78xx/test_mcs51_adc_refchain.cpp)
    target_compile_definitions(test_mcs51_adc_refchain PRIVATE WINK_MCU_CMS8S78XX=1)

    # Stage 3 A-05: GPIO direction modeling — TRIS-gated suppression,
    # pull-up HiZ default, analog digital-read mask (classic passthrough).
    add_mcs51_host_test(test_mcs51_gpio_dir
        ../frameworks/mcs51/test/cms8s78xx/test_mcs51_gpio_dir.cpp)

    # Stage 3 A-03 (ADR-0081): UART TX per-byte synchronous charge — baud
    # anchors per source, frame widths, 22B scale, no-charge paths.
    add_mcs51_host_test(test_mcs51_uart_charge
        ../frameworks/mcs51/test/cms8s78xx/test_mcs51_uart_charge.cpp)
    # A-03 STRICT twin: same TU, STRICT-built compat lib (death test for the
    # uncomputable-source abort). Mirrors the uart_tx_ready_strict block.
    add_wink_host_test(test_mcs51_uart_charge_strict
        ../frameworks/mcs51/test/cms8s78xx/test_mcs51_uart_charge.cpp)
    target_link_libraries(test_mcs51_uart_charge_strict PRIVATE wink_mcs51_core_strict wink_mcs51_cms8s_strict wink_mcs51_at89 wink_mcs51_adc0832 wink_runtime
        wink_mcs51_cms8s_register)
    target_compile_definitions(test_mcs51_uart_charge_strict PRIVATE SIMULATION=1 WINK_MCU_AT89C52=1)
    if(NOT MSVC)
        target_compile_options(test_mcs51_uart_charge_strict PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:-std=c++17 -Wno-write-strings
                -fno-exceptions -fno-rtti -fno-threadsafe-statics>)
    else()
        target_compile_options(test_mcs51_uart_charge_strict PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:/std:c++17 /utf-8 /wd4101 /wd4245>)
    endif()
        target_include_directories(test_mcs51_uart_charge_strict PRIVATE ${_MCS51_FW_DIR}/include)
        target_include_directories(test_mcs51_uart_charge_strict PRIVATE
            ${_MCS51_FW_DIR}/chips/cms8s78xx/include
            ${_MCS51_FW_DIR}/chips/at89c52/include)
        target_include_directories(test_mcs51_uart_charge_strict PRIVATE ${_MCS51_FW_DIR}/test)
    if(TARGET generate_config)
        add_dependencies(test_mcs51_uart_charge_strict generate_config)
    endif()

    # Stage 3 A-04 (GAP-07): WDT coarse model + TA narrowing — WTS intervals,
    # arming/feed lifecycle, health_pot scale, interleave/timeout rollback.
    add_mcs51_host_test(test_mcs51_wdt_ta
        ../frameworks/mcs51/test/cms8s78xx/test_mcs51_wdt_ta.cpp)

    # A-04 STRICT twin: same TU, STRICT-built compat lib (overflow abort).
    add_wink_host_test(test_mcs51_wdt_ta_strict
        ../frameworks/mcs51/test/cms8s78xx/test_mcs51_wdt_ta.cpp)
    target_link_libraries(test_mcs51_wdt_ta_strict PRIVATE wink_mcs51_core_strict wink_mcs51_cms8s_strict wink_mcs51_at89 wink_mcs51_adc0832 wink_runtime
        wink_mcs51_cms8s_register)
    target_compile_definitions(test_mcs51_wdt_ta_strict PRIVATE SIMULATION=1 WINK_MCU_AT89C52=1)
    if(NOT MSVC)
        target_compile_options(test_mcs51_wdt_ta_strict PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:-std=c++17 -Wno-write-strings
                -fno-exceptions -fno-rtti -fno-threadsafe-statics>)
    else()
        target_compile_options(test_mcs51_wdt_ta_strict PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:/std:c++17 /utf-8 /wd4101 /wd4245>)
    endif()
        target_include_directories(test_mcs51_wdt_ta_strict PRIVATE ${_MCS51_FW_DIR}/include)
        target_include_directories(test_mcs51_wdt_ta_strict PRIVATE
            ${_MCS51_FW_DIR}/chips/cms8s78xx/include
            ${_MCS51_FW_DIR}/chips/at89c52/include)
        target_include_directories(test_mcs51_wdt_ta_strict PRIVATE ${_MCS51_FW_DIR}/test)
    if(TARGET generate_config)
        add_dependencies(test_mcs51_wdt_ta_strict generate_config)
    endif()

    # Stage 3 A-08 (GAP-24): classic external MOVX bus conflict + IAP trap —
    # XBYTE/GPIO both orders, P1 exemption, CMS8S zero-regression, IAP block.
    # S2-1: TU renamed classic_bus -> ext_bus (generic concept); the ctest
    # NAME stays test_mcs51_classic_bus so the migration baseline
    # (--compare name set) is untouched.
    add_mcs51_host_test(test_mcs51_classic_bus
        ../frameworks/mcs51/test/core/test_mcs51_ext_bus.cpp)

    # GAP-24 STRICT twin: same TU, STRICT-built compat lib (conflict + IAP
    # abort via child re-execution). Mirrors the wdt_ta_strict block.
    add_wink_host_test(test_mcs51_classic_bus_strict
        ../frameworks/mcs51/test/core/test_mcs51_ext_bus.cpp)
    target_link_libraries(test_mcs51_classic_bus_strict PRIVATE wink_mcs51_core_strict wink_mcs51_cms8s_strict wink_mcs51_at89 wink_mcs51_adc0832 wink_runtime
        wink_mcs51_cms8s_register)
    target_compile_definitions(test_mcs51_classic_bus_strict PRIVATE SIMULATION=1 WINK_MCU_AT89C52=1)
    if(NOT MSVC)
        target_compile_options(test_mcs51_classic_bus_strict PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:-std=c++17 -Wno-write-strings
                -fno-exceptions -fno-rtti -fno-threadsafe-statics>)
    else()
        target_compile_options(test_mcs51_classic_bus_strict PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:/std:c++17 /utf-8 /wd4101 /wd4245>)
    endif()
        target_include_directories(test_mcs51_classic_bus_strict PRIVATE ${_MCS51_FW_DIR}/include)
        target_include_directories(test_mcs51_classic_bus_strict PRIVATE
            ${_MCS51_FW_DIR}/chips/cms8s78xx/include
            ${_MCS51_FW_DIR}/chips/at89c52/include)
        target_include_directories(test_mcs51_classic_bus_strict PRIVATE ${_MCS51_FW_DIR}/test)
    if(TARGET generate_config)
        add_dependencies(test_mcs51_classic_bus_strict generate_config)
    endif()

    # M6: NTC closed-loop thermostat through the board-codegen ADC0832 seam.
    # Stage3: the driver binds the codegen pins itself in its post-init hook
    # (adc0832 compat init from mcs51_board_config.h — the bridge no longer
    # auto-binds) and injects NTC codes. Asserts heater latch toggle at the
    # setpoint and open/short safe states (acceptance #4). Gated on codegen.
    if(_MCS51_HAS_BOARD_CONFIG)
        add_mcs51_host_test(test_mcs51_iron_ntc_e2e
            ../frameworks/mcs51/test/core/test_mcs51_iron_ntc_e2e.c
            ${_MCS51_iron_ntc_CPP}
            ${_MCS51_RUNTIME_SRCS})
        target_include_directories(test_mcs51_iron_ntc_e2e PRIVATE ${_MCS51_GEN_DIR})
        add_dependencies(test_mcs51_iron_ntc_e2e generate_mcs51_board_config)
    endif()

    # ── M5 tier-b harvest: compile+RUN the UNMODIFIED vendor StdDriver adc.c ──
    # The vendor SDK is a reference-only fixture under docs/vendors (never
    # committed). When present, transcode its GBK adc.h to UTF-8 and run the
    # Keil-dialect cleanup on adc.c in the build tree; the sandbox shim
    # cms8s78xx.h (framework include) shadows the vendor Keil device header.
    # Skips gracefully when the fixture is absent (plain-C/CI checkouts).
    set(_MCS51_VENDOR_STD
        "${CMAKE_CURRENT_SOURCE_DIR}/../../docs/vendors/Cmsemicon/CMS8S78xx_DemoCode_V2.0.2/CMS8S78xx_Demo/Libary/StdDriver")
    set(_MCS51_VENDOR_ADC_C "${_MCS51_VENDOR_STD}/src/adc.c")
    if(EXISTS "${_MCS51_VENDOR_ADC_C}")
        set(_MCS51_VENDOR_INC_DIR ${_MCS51_GEN_DIR}/vendor_inc)
        set(_MCS51_VENDOR_ADC_H  ${_MCS51_VENDOR_INC_DIR}/adc.h)
        set(_MCS51_VENDOR_ADC_CPP ${_MCS51_GEN_DIR}/vendor_adc.cpp)

        # GBK vendor header -> UTF-8 build-tree copy (drivers #include "adc.h").
        add_custom_command(
            OUTPUT ${_MCS51_VENDOR_ADC_H}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${_MCS51_VENDOR_INC_DIR}
            COMMAND ${Python3_EXECUTABLE}
                ${_MCS51_FW_DIR}/tools/mcs51_cleanup.py --transcode
                ${_MCS51_VENDOR_STD}/inc/adc.h ${_MCS51_VENDOR_ADC_H}
            DEPENDS ${_MCS51_VENDOR_STD}/inc/adc.h
                    ${_MCS51_FW_DIR}/tools/mcs51_cleanup.py
            COMMENT "mcs51 vendor: transcode StdDriver/inc/adc.h (GBK->UTF-8)"
            VERBATIM)

        # Unmodified vendor adc.c -> cleaned .cpp (ISR rewrite; GBK->UTF-8).
        add_custom_command(
            OUTPUT ${_MCS51_VENDOR_ADC_CPP}
            COMMAND ${Python3_EXECUTABLE}
                ${_MCS51_FW_DIR}/tools/mcs51_cleanup.py
                ${_MCS51_VENDOR_ADC_C} ${_MCS51_VENDOR_ADC_CPP}
            DEPENDS ${_MCS51_VENDOR_ADC_C} ${_MCS51_VENDOR_ADC_H}
                    ${_MCS51_FW_DIR}/tools/mcs51_cleanup.py
            COMMENT "mcs51 vendor cleanup: StdDriver/src/adc.c -> vendor_adc.cpp"
            VERBATIM)

        # Host-only: the tier-b driver is a direct model unit test (own main,
        # no fiber super-loop); the wasm harness already covers the same model
        # via the cms8s_adc e2e sample.
        add_mcs51_host_test(test_mcs51_cms8s_vendor
            ../frameworks/mcs51/test/cms8s78xx/test_cms8s_vendor_stdriver.cpp
            ${_MCS51_VENDOR_ADC_CPP})
        # SYSTEM: the transcoded vendor adc.h is third-party code we must not
        # edit (its banner block-comments re-open "/*" inside a comment, and it
        # re-#defines the enum macros). GCC/Clang compile -isystem headers with
        # warnings suppressed (incl. -Wcomment and macro redefinition), so our
        # own TUs stay under -Werror without silencing anything in our code.
        target_include_directories(test_mcs51_cms8s_vendor SYSTEM PRIVATE
            ${_MCS51_VENDOR_INC_DIR})
        # The vendor adc.c TU defines the StdDriver ADC_* functions itself;
        # suppress the header's file-local shim copies (REG_CMS8S78XX.H §9) to
        # avoid a static/extern redefinition clash.
        target_compile_definitions(test_mcs51_cms8s_vendor PRIVATE WINK_CMS8S_VENDOR_ADC=1)
        if(MSVC)
            # /wd4005: vendor adc.h re-#defines the enum macros (ADC_RESULT_*,
            # ADC_VREF_*, ADC_GO ...) that REG_CMS8S.H also defines. They are
            # token-identical (legal redefinition) — the shim intentionally
            # mirrors the vendor names so StdDriver compiles unmodified.
            target_compile_options(test_mcs51_cms8s_vendor PRIVATE /wd4005)
        endif()
    else()
        message(STATUS "mcs51 tier-b: vendor StdDriver fixture not found "
            "(docs/vendors/Cmsemicon) — skipping test_mcs51_cms8s_vendor")
    endif()
endif()

add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Wasm-side ADC unit test (emcc + Node). Gracefully skips when emcc or node is
# not on PATH. Registered as ctest label "wasm".
include(${CMAKE_CURRENT_SOURCE_DIR}/wasm/pal_adc/add_wink_wasm_adc_test.cmake)

# MCS-51/8051 interception wasm test (emcc + ASYNCIFY fibers + Node).
# Gracefully skips when emcc/node absent.
include(${CMAKE_CURRENT_SOURCE_DIR}/../frameworks/mcs51/test/wasm/add_wink_wasm_mcs51_test.cmake)
