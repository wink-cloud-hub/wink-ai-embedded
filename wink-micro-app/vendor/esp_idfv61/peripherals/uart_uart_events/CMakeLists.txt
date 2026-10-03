# SPDX-License-Identifier: Apache-2.0
# wink-micro-app/vendor/esp_idfv61/peripherals/uart_uart_events/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(esp_idfv61_uart_uart_events C)

if(EMSCRIPTEN)
    set(WINK_APP_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/uart_events_example_main.c
        PARENT_SCOPE
    )
    set(WINK_APP_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        PARENT_SCOPE
    )
    set(WINK_APP_ESP_IDF TRUE PARENT_SCOPE)
    return()
endif()

message(STATUS "esp_idfv61_uart_uart_events: wasm-sim only — no host/esp32 target produced.")
set(WINK_APP_SOURCES "" PARENT_SCOPE)
