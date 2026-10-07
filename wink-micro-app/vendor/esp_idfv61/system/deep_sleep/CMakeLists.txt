# SPDX-License-Identifier: Apache-2.0
# wink-micro-app/vendor/esp_idfv61/system/deep_sleep/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(esp_idfv61_system_deep_sleep C)

if(EMSCRIPTEN)
    set(WINK_APP_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/deep_sleep_example_main.c
        ${CMAKE_CURRENT_SOURCE_DIR}/ext_wakeup.c
        ${CMAKE_CURRENT_SOURCE_DIR}/gpio_wakeup.c
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

message(STATUS "esp_idfv61_system_deep_sleep: wasm-sim only — no host/esp32 target produced.")
set(WINK_APP_SOURCES "" PARENT_SCOPE)
