cmake_minimum_required(VERSION 3.16)

# Wink-micro-os static target selection. This firmware builds the ESP32 port,
# so force the variable the wink component CMakeLists gates on (targets/esp32/CMakeLists.txt).
set(TARGET_PLATFORM esp32 CACHE STRING "wink-micro-os target platform" FORCE)

# Register the wink-micro-os ESP32 port as an IDF component.
# NOTE: EXTRA_COMPONENT_DIRS MUST be set before project() or ESP-IDF won't scan it.
if(NOT DEFINED WINK_SDK_PATH)
    if(DEFINED ENV{WINK_SDK_PATH})
        set(WINK_SDK_PATH $ENV{WINK_SDK_PATH})
    else()
        set(WINK_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../wink-micro-os")
    endif()
endif()
set(EXTRA_COMPONENT_DIRS "${WINK_SDK_PATH}/targets/esp32" "${WINK_SDK_PATH}/frameworks/arduino")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(wink_esp32_firmware)
