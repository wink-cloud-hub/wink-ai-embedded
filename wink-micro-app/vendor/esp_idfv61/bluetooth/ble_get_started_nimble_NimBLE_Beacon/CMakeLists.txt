# SPDX-License-Identifier: Apache-2.0
# wink-micro-app/vendor/esp_idfv61/bluetooth/ble_get_started_nimble_NimBLE_Beacon/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(esp_idfv61_ble_get_started_nimble_NimBLE_Beacon C)

if(EMSCRIPTEN)
    set(WINK_APP_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/main.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/gap.c
        PARENT_SCOPE
    )
    set(WINK_APP_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        PARENT_SCOPE
    )
    set(WINK_APP_ESP_IDF TRUE PARENT_SCOPE)
    return()
endif()

message(STATUS "esp_idfv61_ble_get_started_nimble_NimBLE_Beacon: wasm-sim only — no host/esp32 target produced.")
set(WINK_APP_SOURCES "" PARENT_SCOPE)
