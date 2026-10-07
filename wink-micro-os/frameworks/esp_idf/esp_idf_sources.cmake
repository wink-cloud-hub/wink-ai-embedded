# SPDX-License-Identifier: LGPL-3.0-only
set(ESP_IDF_FRAMEWORK_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/src/esp_idf_runtime.c
    ${CMAKE_CURRENT_LIST_DIR}/src/esp_idf_bridge.c
    ${CMAKE_CURRENT_LIST_DIR}/src/core/esp_err.c
    ${CMAKE_CURRENT_LIST_DIR}/src/core/esp_log.c
    ${CMAKE_CURRENT_LIST_DIR}/src/core/esp_system.c
    ${CMAKE_CURRENT_LIST_DIR}/src/core/esp_timer.c
    ${CMAKE_CURRENT_LIST_DIR}/src/core/esp_console.c
    ${CMAKE_CURRENT_LIST_DIR}/src/core/esp_task_wdt.c
    ${CMAKE_CURRENT_LIST_DIR}/src/core/esp_heap_caps.c
    ${CMAKE_CURRENT_LIST_DIR}/src/core/esp_sim_handle.c
    ${CMAKE_CURRENT_LIST_DIR}/src/core/esp_fault.c
    ${CMAKE_CURRENT_LIST_DIR}/src/drivers/esp_gpio.c
    ${CMAKE_CURRENT_LIST_DIR}/src/drivers/esp_ledc.c
    ${CMAKE_CURRENT_LIST_DIR}/src/drivers/esp_i2c_legacy.c
    ${CMAKE_CURRENT_LIST_DIR}/src/drivers/esp_i2c_master.c
    ${CMAKE_CURRENT_LIST_DIR}/src/drivers/esp_uart.c
    ${CMAKE_CURRENT_LIST_DIR}/src/drivers/esp_gptimer.c
    ${CMAKE_CURRENT_LIST_DIR}/src/drivers/esp_spi.c
    ${CMAKE_CURRENT_LIST_DIR}/src/drivers/esp_adc.c
    ${CMAKE_CURRENT_LIST_DIR}/src/drivers/esp_dac.c
    ${CMAKE_CURRENT_LIST_DIR}/src/drivers/esp_dedic_gpio.c
    ${CMAKE_CURRENT_LIST_DIR}/src/core/esp_nvs.c
    ${CMAKE_CURRENT_LIST_DIR}/src/core/esp_partition.c
    ${CMAKE_CURRENT_LIST_DIR}/src/core/esp_vfs_ram.c
    ${CMAKE_CURRENT_LIST_DIR}/src/core/esp_spiffs.c
    ${CMAKE_CURRENT_LIST_DIR}/src/freertos/freertos_task.c
    ${CMAKE_CURRENT_LIST_DIR}/src/freertos/freertos_queue.c
    ${CMAKE_CURRENT_LIST_DIR}/src/freertos/freertos_semphr.c
    ${CMAKE_CURRENT_LIST_DIR}/src/freertos/freertos_event.c
    ${CMAKE_CURRENT_LIST_DIR}/src/freertos/freertos_timers.c
    ${CMAKE_CURRENT_LIST_DIR}/src/freertos/freertos_spinlock.c
    ${CMAKE_CURRENT_LIST_DIR}/src/core/esp_event.c
    ${CMAKE_CURRENT_LIST_DIR}/src/wifi/esp_wifi.c
    ${CMAKE_CURRENT_LIST_DIR}/src/wifi/esp_netif.c
    ${CMAKE_CURRENT_LIST_DIR}/src/wifi/sim_wifi_env.c
    ${CMAKE_CURRENT_LIST_DIR}/src/network/sim_network_broker.c
    ${CMAKE_CURRENT_LIST_DIR}/src/network/esp_mqtt.c
    ${CMAKE_CURRENT_LIST_DIR}/src/network/esp_http.c
    ${CMAKE_CURRENT_LIST_DIR}/src/network/sim_bounded_stream.c
    ${CMAKE_CURRENT_LIST_DIR}/src/network/sim_net_responder.c
    ${CMAKE_CURRENT_LIST_DIR}/src/network/esp_http_server.c
    ${CMAKE_CURRENT_LIST_DIR}/src/network/esp_sntp.c
    ${CMAKE_CURRENT_LIST_DIR}/src/network/esp_sockets.c
    ${CMAKE_CURRENT_LIST_DIR}/src/bluetooth/esp_nimble.c
)

include(${CMAKE_CURRENT_LIST_DIR}/esp_idf_target.cmake)

# 对外暴露的公共头目录（PUBLIC）：仅允许厂商公开 API 垫片、目标芯片头与 sdkconfig 默认垫片
set(ESP_IDF_FRAMEWORK_INCLUDES
    ${WINK_ESP_TARGET_INCLUDE_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/shim/include
)

# 门面内部专用私有头目录（PRIVATE）：严格收敛，禁止向应用泄漏
set(ESP_IDF_FRAMEWORK_PRIVATE_INCLUDES
    ${CMAKE_CURRENT_LIST_DIR}/src/freertos
    ${CMAKE_CURRENT_LIST_DIR}/src/core
    ${CMAKE_CURRENT_LIST_DIR}/src/wifi
    ${CMAKE_CURRENT_LIST_DIR}/src/network
    ${CMAKE_CURRENT_LIST_DIR}/src/sim_include
    ${CMAKE_CURRENT_LIST_DIR}/../../targets/common/include
    ${CMAKE_CURRENT_LIST_DIR}/../../trace/include
    ${CMAKE_CURRENT_LIST_DIR}/../../runtime/include
)
