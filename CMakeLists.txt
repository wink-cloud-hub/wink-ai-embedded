# wink-micro-os 顶层 CMakeLists.txt
#
# 构建配置：
# - host:   cmake -B build-host -DTARGET_PLATFORM=host
# - wasm:   emcmake cmake -B build-wasm -DTARGET_PLATFORM=wasm
# - esp32:  idf.py build (由 ESP-IDF 构建系统接管)

cmake_minimum_required(VERSION 3.15)

# ESP-IDF 组件模式：在 idf_component_register 之前设置项目变量
if(DEFINED ENV{IDF_PATH} OR ESP_PLATFORM)
    # ESP-IDF 环境：作为 IDF component 参与构建
    message(STATUS "Building as ESP-IDF component")
    # 此处的变量会被 wink-micro-os/CMakeLists.txt 继承
    set(TARGET_PLATFORM "esp32" CACHE STRING "")
    add_subdirectory(wink-micro-os)
else()
    # 非 ESP-IDF 环境：host/wasm 独立构建
    project(wink-micro-os-top C CXX)
    message(STATUS "Standalone build - TARGET_PLATFORM not specified, defaulting to host")
    add_subdirectory(wink-micro-os)
endif()
