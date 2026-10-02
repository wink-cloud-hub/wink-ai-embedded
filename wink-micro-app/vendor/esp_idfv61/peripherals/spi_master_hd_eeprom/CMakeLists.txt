# SPDX-License-Identifier: Apache-2.0
# wink-micro-app/vendor/esp_idfv61/peripherals/spi_master_hd_eeprom/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(esp_idfv61_spi_master_hd_eeprom C)

if(EMSCRIPTEN)
    set(WINK_APP_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/spi_eeprom_main.c
        ${CMAKE_CURRENT_SOURCE_DIR}/spi_eeprom.c
        PARENT_SCOPE
    )
    set(WINK_APP_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        PARENT_SCOPE
    )
    set(WINK_APP_ESP_IDF TRUE PARENT_SCOPE)
    return()
endif()

message(STATUS "esp_idfv61_spi_master_hd_eeprom: wasm-sim only — no host/esp32 target produced.")
set(WINK_APP_SOURCES "" PARENT_SCOPE)
