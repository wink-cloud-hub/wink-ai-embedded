# SPDX-License-Identifier: Apache-2.0
# wink-micro-app/vendor/esp_idfv61/protocols/sntp/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(esp_idfv61_protocols_sntp C)

if(EMSCRIPTEN)
    set(WINK_APP_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/sntp_example_main.c
        PARENT_SCOPE
    )
    set(WINK_APP_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        PARENT_SCOPE
    )
    set(WINK_APP_ESP_IDF TRUE PARENT_SCOPE)
    return()
endif()

message(STATUS "esp_idfv61_protocols_sntp: wasm-sim only — no host/esp32 target produced.")
set(WINK_APP_SOURCES "" PARENT_SCOPE)
