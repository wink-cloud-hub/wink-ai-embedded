# wink-micro-os top-level CMakeLists.txt
#
# Build configuration:
# - host:   cmake -B build-host -DTARGET_PLATFORM=host
# - wasm:   emcmake cmake -B build-wasm -DTARGET_PLATFORM=wasm
# - esp32:  idf.py build (handled by ESP-IDF build system)

cmake_minimum_required(VERSION 3.15)

# ESP-IDF component mode check
if(DEFINED ENV{IDF_PATH} OR ESP_PLATFORM)
    # ESP-IDF environment: build as IDF component
    message(STATUS "Building as ESP-IDF component")
    set(TARGET_PLATFORM "esp32" CACHE STRING "")
    add_subdirectory(wink-micro-os)
else()
    # Standalone build (host/wasm)
    project(wink-micro-os-top C CXX)
    message(STATUS "Standalone build - TARGET_PLATFORM not specified, defaulting to host")
    add_subdirectory(wink-micro-os)
endif()
