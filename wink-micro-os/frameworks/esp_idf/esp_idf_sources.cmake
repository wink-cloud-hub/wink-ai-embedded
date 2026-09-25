# SPDX-License-Identifier: LGPL-3.0-only
set(ESP_IDF_FRAMEWORK_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/src/esp_idf_runtime.c
    ${CMAKE_CURRENT_LIST_DIR}/src/esp_idf_bridge.c
    ${CMAKE_CURRENT_LIST_DIR}/src/core/esp_err.c
    ${CMAKE_CURRENT_LIST_DIR}/src/core/esp_log.c
    ${CMAKE_CURRENT_LIST_DIR}/src/core/esp_system.c
    ${CMAKE_CURRENT_LIST_DIR}/src/drivers/esp_gpio.c
    ${CMAKE_CURRENT_LIST_DIR}/src/drivers/esp_ledc.c
    ${CMAKE_CURRENT_LIST_DIR}/src/drivers/esp_i2c_legacy.c
    ${CMAKE_CURRENT_LIST_DIR}/src/drivers/esp_i2c_master.c
    ${CMAKE_CURRENT_LIST_DIR}/src/drivers/esp_uart.c
    ${CMAKE_CURRENT_LIST_DIR}/src/drivers/esp_gptimer.c
    ${CMAKE_CURRENT_LIST_DIR}/src/drivers/esp_spi.c
    ${CMAKE_CURRENT_LIST_DIR}/src/core/esp_nvs.c
    ${CMAKE_CURRENT_LIST_DIR}/src/freertos/freertos_task.c
    ${CMAKE_CURRENT_LIST_DIR}/src/freertos/freertos_queue.c
    ${CMAKE_CURRENT_LIST_DIR}/src/freertos/freertos_semphr.c
    ${CMAKE_CURRENT_LIST_DIR}/src/freertos/freertos_event.c
    ${CMAKE_CURRENT_LIST_DIR}/src/freertos/freertos_timers.c
)

if(NOT DEFINED WINK_ESP_TARGET)
    set(WINK_ESP_TARGET "esp32")
endif()

set(ESP_IDF_FRAMEWORK_INCLUDES
    ${CMAKE_CURRENT_LIST_DIR}/chips/${WINK_ESP_TARGET}/include
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/src/freertos
    ${CMAKE_CURRENT_LIST_DIR}/../../targets/common/include
    # 手写 esp_check.h -> wink_runtime.h -> wink_fault.h -> wink_trace.h 依赖链
    ${CMAKE_CURRENT_LIST_DIR}/../../trace/include
    # 默认 sdkconfig 垫片：必须排在 corpus overlay 之后（见 shim/include/sdkconfig.h）
    ${CMAKE_CURRENT_LIST_DIR}/shim/include
)
