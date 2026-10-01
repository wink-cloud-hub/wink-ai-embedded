# SPDX-License-Identifier: Apache-2.0
# wink-micro-app/vendor/esp_idfv61/peripherals/uart_echo/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(esp_idfv61_uart_echo C)

if(EMSCRIPTEN)
    set(WINK_APP_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/uart_echo_example_main.c
        PARENT_SCOPE
    )
    set(WINK_APP_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        PARENT_SCOPE
    )
    set(WINK_APP_ESP_IDF TRUE PARENT_SCOPE)
    return()
endif()

message(STATUS "esp_idfv61_uart_echo: wasm-sim only — no host/esp32 target produced.")
set(WINK_APP_SOURCES "" PARENT_SCOPE)
