# SPDX-License-Identifier: Apache-2.0
# wink-micro-app/vendor/esp_idfv61/peripherals/ledc_ledc_fade/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(esp_idfv61_ledc_fade C)

if(EMSCRIPTEN)
    set(WINK_APP_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/ledc_fade_example_main.c
        PARENT_SCOPE
    )
    set(WINK_APP_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        PARENT_SCOPE
    )
    set(WINK_APP_ESP_IDF TRUE PARENT_SCOPE)
    return()
endif()

message(STATUS "esp_idfv61_ledc_fade: wasm-sim only — no host/esp32 target produced.")
set(WINK_APP_SOURCES "" PARENT_SCOPE)
