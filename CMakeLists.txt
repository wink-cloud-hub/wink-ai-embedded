# wink-micro-os top-level CMakeLists.txt (Layout v2 Contract)
#
# Build configuration:
# - host:   cmake -B build/tests/host/general -DTARGET_PLATFORM=host -DWINK_BUILD_TESTS=ON
# - wasm:   emcmake cmake -S wink-micro-os -B build/wasm/<app-id> -DTARGET_PLATFORM=wasm
# - esp32:  python wink-tools/wink.py esp32 --app <app-id>

cmake_minimum_required(VERSION 3.15)

# Layout v2 Guard: Prevent writing CMakeCache directly to root build/ container
get_filename_component(_BINARY_DIR_REAL "${CMAKE_BINARY_DIR}" REALPATH)
get_filename_component(_SOURCE_DIR_REAL "${CMAKE_SOURCE_DIR}" REALPATH)
if(_BINARY_DIR_REAL STREQUAL "${_SOURCE_DIR_REAL}/build")
    message(FATAL_ERROR
        "Build Layout Violation: 'build/' is a container directory and cannot be used directly as CMake binary dir.\n"
        "Please use an isolated leaf directory, e.g.:\n"
        "  cmake -B build/tests/host/general -DTARGET_PLATFORM=host\n"
        "  or use Wink CLI: python wink-tools/wink.py test / build"
    )
endif()

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
