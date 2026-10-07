# SPDX-License-Identifier: Apache-2.0
# wink-micro-app/vendor/esp_idfv61/protocols/http_server_restful_server/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(esp_idfv61_http_server_restful_server C)

if(EMSCRIPTEN)
    set(WINK_APP_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/esp_rest_main.c
        ${CMAKE_CURRENT_SOURCE_DIR}/rest_server.c
        ${CMAKE_CURRENT_SOURCE_DIR}/support/cJSON.c
        PARENT_SCOPE
    )
    set(WINK_APP_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        PARENT_SCOPE
    )
    set(WINK_APP_ESP_IDF TRUE PARENT_SCOPE)
    return()
endif()

message(STATUS "esp_idfv61_http_server_restful_server: wasm-sim only — no host/esp32 target produced.")
set(WINK_APP_SOURCES "" PARENT_SCOPE)
