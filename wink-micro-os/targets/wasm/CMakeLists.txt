# Wasm simulation port build configuration (Emscripten environment only).

set(PAL_WASM_SOURCES
    # HAL layer
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_hal_wasm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_log_wasm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_irq_wasm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_storage_wasm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_wasm_adc.c
    # Wasm extensions
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_wasm_physical.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_wasm_fault.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pal_wasm_fault_domain.c
    ${CMAKE_CURRENT_SOURCE_DIR}/devices/wasm_sim_registry.c
    # Target independent common sources
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/pal_resource.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/wink_sim_physical.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/src/wink_sim_scheduler.c
)

if(WINK_USE_RC_SERVO)
    list(APPEND PAL_WASM_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/devices/wasm_dev_servo.c)
endif()

if(WINK_USE_ULTRASONIC)
    list(APPEND PAL_WASM_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/devices/wasm_dev_ultrasonic.c)
endif()

set(PAL_WASM_SOURCES ${PAL_WASM_SOURCES} PARENT_SCOPE)

set(WASM_ENTRY_SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/wasm_entry.c
    PARENT_SCOPE)

set(WASM_EXTRA_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
    PARENT_SCOPE)
