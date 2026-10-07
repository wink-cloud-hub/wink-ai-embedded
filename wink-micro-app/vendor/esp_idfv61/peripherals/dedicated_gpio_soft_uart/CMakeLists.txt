# SPDX-License-Identifier: Apache-2.0
# wink-micro-app/vendor/esp_idfv61/peripherals/dedicated_gpio_soft_uart/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(esp_idfv61_peripherals_dedicated_gpio_soft_uart C)

if(EMSCRIPTEN)
    set(WINK_APP_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/soft_uart_main.c
        ${CMAKE_CURRENT_SOURCE_DIR}/components/soft_uart/soft_uart.c
        PARENT_SCOPE
    )
    set(WINK_APP_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/components/soft_uart/include
        PARENT_SCOPE
    )
    set(WINK_APP_ESP_IDF TRUE PARENT_SCOPE)
    return()
endif()

message(STATUS "esp_idfv61_peripherals_dedicated_gpio_soft_uart: wasm-sim only — no host/esp32 target produced.")
set(WINK_APP_SOURCES "" PARENT_SCOPE)
