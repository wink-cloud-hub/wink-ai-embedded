# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20)
project(esp_idf_h6_handles C)

if(EMSCRIPTEN)
    set(WINK_APP_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/app_main.c PARENT_SCOPE)
    set(WINK_APP_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include PARENT_SCOPE)
    set(WINK_APP_ESP_IDF TRUE PARENT_SCOPE)
    return()
endif()

message(STATUS "esp_idf_h6_handles: wasm-sim only")
set(WINK_APP_SOURCES "" PARENT_SCOPE)
