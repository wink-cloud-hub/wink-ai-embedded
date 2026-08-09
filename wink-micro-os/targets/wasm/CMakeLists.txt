# Wasm simulation port build configuration (Emscripten environment only).

set(PAL_WASM_SOURCES
    # Core Modular Channels (Axis A)
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_wasm_ch1_gpio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_wasm_ch2_bus.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_wasm_ch2_uart.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_wasm_ch2b_pwm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_wasm_ch3_adc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_wasm_ch4_buffer.c
    # Support & Fault Domain (Axis F)
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_log_wasm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_wasm_irq.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_storage_wasm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_wasm_degradation.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_wasm_fault.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_wasm_fault_domain.c
    # Target independent common sources
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/pal_resource.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/wink_sim_physical.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/wink_sim_scheduler.c
)

set(PAL_WASM_SOURCES ${PAL_WASM_SOURCES} PARENT_SCOPE)

set(WASM_ENTRY_SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/wasm_entry.c
    PARENT_SCOPE)

set(WASM_EXTRA_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
    PARENT_SCOPE)
