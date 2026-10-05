# SPDX-License-Identifier: Apache-2.0
# wink-micro-app/vendor/esp_idfv61/system/freertos_basic_freertos_smp_usage/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(esp_idfv61_system_freertos_basic_freertos_smp_usage C)

if(EMSCRIPTEN)
    set(WINK_APP_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/basic_freertos_smp_usage.c
        ${CMAKE_CURRENT_SOURCE_DIR}/create_task_example.c
        ${CMAKE_CURRENT_SOURCE_DIR}/queue_example.c
        ${CMAKE_CURRENT_SOURCE_DIR}/lock_example.c
        ${CMAKE_CURRENT_SOURCE_DIR}/task_notify_example.c
        ${CMAKE_CURRENT_SOURCE_DIR}/batch_processing_example.c
        PARENT_SCOPE
    )
    set(WINK_APP_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        PARENT_SCOPE
    )
    set(WINK_APP_ESP_IDF TRUE PARENT_SCOPE)
    return()
endif()

message(STATUS "esp_idfv61_system_freertos_basic_freertos_smp_usage: wasm-sim only — no host/esp32 target produced.")
set(WINK_APP_SOURCES "" PARENT_SCOPE)
