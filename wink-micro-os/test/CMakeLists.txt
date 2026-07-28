# wink-micro-os host unit tests (PC gcc, no ESP-IDF, no Emscripten)

set(UNITY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/unity)

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
add_wink_host_test(test_dal_servo unit/dal/test_dal_servo.c)
add_wink_host_test(test_dal_led unit/dal/test_dal_led.c)
add_wink_host_test(test_dal_button unit/dal/test_dal_button.c)
add_wink_host_test(test_dal_ssd1306 unit/dal/test_dal_ssd1306.c)
add_wink_host_test(test_dal_ultrasonic unit/dal/test_dal_ultrasonic.c)

add_wink_host_test(test_dal_dc_motor unit/dal/test_dal_dc_motor.c)
target_include_directories(test_dal_dc_motor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/actuator)

add_wink_host_test(test_dal_encoder unit/dal/test_dal_encoder.c)
target_include_directories(test_dal_encoder PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/sensor)

add_wink_host_test(test_dal_eeprom unit/dal/test_dal_eeprom.c)
target_include_directories(test_dal_eeprom PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/storage)

add_wink_host_test(test_dal_gps unit/dal/test_dal_gps.c)
target_include_directories(test_dal_gps PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../dal/include/comm)

add_wink_host_test(test_pal_i2c_bus unit/pal/test_pal_i2c_bus.c)

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

add_wink_host_test(test_bal_servo_sweep unit/bal/test_bal_servo_sweep.c)
target_link_libraries(test_bal_servo_sweep PRIVATE wink_bal)

add_wink_test(test_pid unit/bal/test_pid.c)
target_link_libraries(test_pid PRIVATE wink_bal)

add_wink_host_test(test_bal_closed_loop_motor unit/bal/test_bal_closed_loop_motor.c)
target_link_libraries(test_bal_closed_loop_motor PRIVATE wink_bal)

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

add_wink_host_test(test_wasm_devices_sim unit/sim/test_wasm_devices_sim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../targets/wasm/devices/wasm_sim_registry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../targets/wasm/devices/wasm_dev_servo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../targets/wasm/devices/wasm_dev_ultrasonic.c)

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

add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
