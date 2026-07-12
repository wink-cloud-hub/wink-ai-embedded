cmake_minimum_required(VERSION 3.15)
project(wink-micro-os C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ── App injection ────────────────────────────────────────────────────────────
if(NOT DEFINED WINK_APP_DIR OR WINK_APP_DIR STREQUAL "")
    set(WINK_APP_DIR "wink-micro-app/avoidance_car"
        CACHE PATH "Path to App directory containing wink-app.json")
endif()
get_filename_component(_WINK_WORKSPACE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
if(NOT IS_ABSOLUTE "${WINK_APP_DIR}")
    if(WINK_APP_DIR MATCHES "^[.]")
        get_filename_component(WINK_APP_DIR "${WINK_APP_DIR}" ABSOLUTE
            BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    else()
        get_filename_component(WINK_APP_DIR "${WINK_APP_DIR}" ABSOLUTE
            BASE_DIR "${_WINK_WORKSPACE_ROOT}")
    endif()
else()
    get_filename_component(WINK_APP_DIR "${WINK_APP_DIR}" ABSOLUTE)
endif()
set(WINK_APP_JSON "${WINK_APP_DIR}/wink-app.json")
if(NOT EXISTS "${WINK_APP_JSON}")
    message(FATAL_ERROR
        "WINK_APP_JSON not found: ${WINK_APP_JSON}\n"
        "Set -DWINK_APP_DIR= to a directory that contains wink-app.json.")
endif()
message(STATUS "WINK_APP_DIR = ${WINK_APP_DIR}")
message(STATUS "WINK_APP_JSON = ${WINK_APP_JSON}")

# ── Config generation ────────────────────────────────────────────────────────
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(WINK_CONFIG_DIR ${CMAKE_BINARY_DIR}/generated)
set(WINK_CONFIG_H ${WINK_CONFIG_DIR}/wink_config.h)
file(MAKE_DIRECTORY "${WINK_CONFIG_DIR}")

# Use codegen from SDK tools if available, else from WINK_CODEGEN_ROOT
if(NOT DEFINED WINK_CODEGEN_ROOT OR WINK_CODEGEN_ROOT STREQUAL "")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/codegen/config_h.py")
        set(WINK_CODEGEN_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/tools/codegen")
    else()
        message(FATAL_ERROR "WINK_CODEGEN_ROOT not set and tools/codegen/ not found in SDK.")
    endif()
endif()

add_custom_command(
    OUTPUT ${WINK_CONFIG_H}
    COMMAND ${Python3_EXECUTABLE} ${WINK_CODEGEN_ROOT}/config_h.py
            --input ${WINK_APP_JSON}
            --output ${WINK_CONFIG_H}
            --target ${TARGET_PLATFORM}
            --sdk-mode binary
    DEPENDS ${WINK_APP_JSON}
            ${WINK_CODEGEN_ROOT}/config_h.py
    COMMENT "Generating wink_config.h from ${WINK_APP_JSON}"
    VERBATIM
)
add_custom_target(generate_config DEPENDS ${WINK_CONFIG_H})

# ── Target platform ──────────────────────────────────────────────────────────
set(TARGET_PLATFORM "host" CACHE STRING "Target hardware platform")
add_compile_definitions(PLATFORM_${TARGET_PLATFORM})

# ── Import precompiled binary SDK ────────────────────────────────────────────
# This CMakeLists.txt is the consumer-facing entry point for the Binary SDK.
# It always operates in BINARY mode — the precompiled .a is imported and
# backward-compatible INTERFACE aliases are created.
include(${CMAKE_CURRENT_SOURCE_DIR}/targets/host/wink_binary_import.cmake)
message(STATUS "WINK_SDK_MODE = binary (Binary SDK consumer entry point)")

# ── App subdirectory ─────────────────────────────────────────────────────────
if(TARGET_PLATFORM STREQUAL "host" OR NOT TARGET_PLATFORM STREQUAL "wasm")
    enable_testing()

    # samples/common (if available)
    if(EXISTS ${WINK_APP_DIR}/../common)
        add_subdirectory(${WINK_APP_DIR}/../common ${CMAKE_BINARY_DIR}/common_build)
    elseif(EXISTS ${WINK_APP_DIR}/common)
        add_subdirectory(${WINK_APP_DIR}/common ${CMAKE_BINARY_DIR}/common_build)
    endif()

    if(EXISTS ${WINK_APP_DIR})
        add_subdirectory(${WINK_APP_DIR} ${CMAKE_BINARY_DIR}/app_build)
    else()
        message(FATAL_ERROR "WINK_APP_DIR does not exist: ${WINK_APP_DIR}")
    endif()

    # Smoke test: verify public headers compile + .a links
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/smoke_test.c")
        add_executable(binary_sdk_smoke smoke_test.c)
        target_link_libraries(binary_sdk_smoke PRIVATE wink_micro_os)
        add_test(NAME binary_sdk_smoke COMMAND binary_sdk_smoke)
    endif()
endif()
