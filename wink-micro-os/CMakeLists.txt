cmake_minimum_required(VERSION 3.15)
project(wink-micro-os C CXX)

# Require C99
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Default to WASM target platform
set(TARGET_PLATFORM "wasm" CACHE STRING "Target hardware platform: wasm, esp32, stm32")
message(STATUS "Configuring wink-micro-os for platform: ${TARGET_PLATFORM}")

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/wink_tools.cmake)
message(STATUS "WINK_TOOLS_ROOT = ${WINK_TOOLS_ROOT}")

# OSAL Decoupling Layer (ADR-0041)
include(cmake/wink_osal.cmake)
add_subdirectory(osal)

# Platform compilation flags
add_compile_definitions(PLATFORM_${TARGET_PLATFORM})
if(TARGET_PLATFORM STREQUAL "wasm")
    add_compile_definitions(SIMULATION=1)
endif()

# Application configuration path resolution
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
    message(STATUS "WINK_APP_JSON not found at: ${WINK_APP_JSON} -- Using default configuration.")
    set(WINK_APP_JSON "")
endif()
message(STATUS "WINK_APP_DIR = ${WINK_APP_DIR}")
message(STATUS "WINK_APP_JSON = ${WINK_APP_JSON}")

# ADR-0039: Host multi-sample shares one `dal` -- do NOT prune from the
# WINK_APP_DIR-derived WINK_APP_JSON (that path is for wink_config.h).
# Opt in for single-app host builds: -DWINK_DAL_BIND_APP_JSON=ON.
# Wasm / other non-host SOURCE builds prune whenever JSON is present.
option(WINK_DAL_BIND_APP_JSON
    "Bind shared host dal pruning to wink-app.json (single-app only; breaks multi-sample)"
    OFF)

# SDK Mode Detection
if(NOT DEFINED WINK_SDK_MODE OR WINK_SDK_MODE STREQUAL "")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/host")
        set(WINK_SDK_MODE "binary")
    else()
        set(WINK_SDK_MODE "source")
    endif()
endif()
message(STATUS "WINK_SDK_MODE = ${WINK_SDK_MODE}")

if(WINK_SDK_MODE STREQUAL "binary" AND NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/host")
    message(FATAL_ERROR
        "WINK_SDK_MODE=binary but libs/host/ not found in SDK tree. "
        "Use a Binary SDK tarball or switch to --sdk-mode source.")
endif()
if(WINK_SDK_MODE STREQUAL "source" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/SDK_MANIFEST.txt")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/SDK_MANIFEST.txt" _sdk_manifest_lines)
    foreach(_line ${_sdk_manifest_lines})
        if(_line MATCHES "^mode=binary")
            message(FATAL_ERROR
                "WINK_SDK_MODE=source but SDK_MANIFEST.txt says mode=binary. "
                "This is a Binary SDK; use --sdk-mode binary.")
        endif()
    endforeach()
endif()

if(WINK_SDK_MODE STREQUAL "binary")
    set(_CONFIG_SDK_MODE_ARGS --sdk-mode binary)
else()
    set(_CONFIG_SDK_MODE_ARGS)
endif()

# Config Codegen Hook (config_h.py)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(WINK_CONFIG_DIR ${CMAKE_BINARY_DIR}/generated)
set(WINK_CONFIG_H ${WINK_CONFIG_DIR}/wink_config.h)

set(_CONFIG_H_DEPS ${WINK_TOOLS_ROOT}/tools/codegen/generators/config_h.py)
if(NOT WINK_APP_JSON STREQUAL "")
    list(APPEND _CONFIG_H_DEPS ${WINK_APP_JSON})
endif()

add_custom_command(
    OUTPUT ${WINK_CONFIG_H}
    COMMAND ${Python3_EXECUTABLE} ${WINK_TOOLS_ROOT}/tools/codegen/generators/config_h.py
            --input "${WINK_APP_JSON}"
            --output ${WINK_CONFIG_H}
            --target ${TARGET_PLATFORM}
            ${_CONFIG_SDK_MODE_ARGS}
    DEPENDS ${_CONFIG_H_DEPS}
    COMMENT "Generating wink_config.h"
    VERBATIM
)

add_custom_target(generate_config DEPENDS ${WINK_CONFIG_H})
message(STATUS "Configuration will be generated at: ${WINK_CONFIG_H}")

# Stack Safety Enforcement (-Wstack-usage / -Wframe-larger-than)
set(WINK_STACK_USAGE_LIMIT 1536 CACHE STRING
    "Per-function stack usage hard limit in bytes (-Wstack-usage)")

if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wstack-usage=${WINK_STACK_USAGE_LIMIT})
elseif(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wframe-larger-than=${WINK_STACK_USAGE_LIMIT})
elseif(MSVC)
    # Enable UTF-8 source/execution character set for MSVC to prevent C4819 encoding warnings/errors
    add_compile_options(/utf-8)
endif()

# C++ Language Options (ADR-0036)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>)
elseif(MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/GR->)
endif()

# Protothread Footgun Detector (ADR-0007)
add_custom_target(
    check_pt_footguns ALL
    COMMAND ${Python3_EXECUTABLE} ${WINK_TOOLS_ROOT}/tools/lint/check_pt_variables.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Scanning for protothread auto variable footguns..."
    VERBATIM
)

# Subdirectory inclusion
if(WINK_SDK_MODE STREQUAL "binary")
    include(${CMAKE_CURRENT_SOURCE_DIR}/targets/host/wink_binary_import.cmake)
else()
    add_subdirectory(pal)
    set(_wink_dal_do_prune FALSE)
    if(TARGET_PLATFORM STREQUAL "host")
        if(WINK_DAL_BIND_APP_JSON)
            set(_wink_dal_do_prune TRUE)
        endif()
    else()
        set(_wink_dal_do_prune TRUE)
    endif()
    if(_wink_dal_do_prune
       AND DEFINED WINK_APP_JSON
       AND NOT "${WINK_APP_JSON}" STREQUAL ""
       AND EXISTS "${WINK_APP_JSON}")
        get_filename_component(WINK_MICRO_OS_ROOT
            "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
        include(${WINK_MICRO_OS_ROOT}/cmake/wink_dal_drivers.cmake)
        wink_dal_apply_pruning("${WINK_APP_JSON}"
            "${CMAKE_BINARY_DIR}/generated")
    endif()
    add_subdirectory(dal)
    add_subdirectory(trace)
    add_subdirectory(runtime)
    add_subdirectory(bal)
    add_subdirectory(frameworks)

    add_dependencies(wink_runtime check_pt_footguns)
endif()

# WebAssembly target configuration
if(TARGET_PLATFORM STREQUAL "wasm" AND EMSCRIPTEN)
    if(WINK_SDK_MODE STREQUAL "binary")
        include(${CMAKE_CURRENT_SOURCE_DIR}/targets/wasm/wink_binary_import.cmake)
        message(STATUS "WINK_SDK_MODE = binary (Wasm Binary SDK consumer)")

        add_subdirectory(${WINK_APP_DIR} ${CMAKE_BINARY_DIR}/app_build)

        if(NOT DEFINED WINK_APP_SOURCES OR "${WINK_APP_SOURCES}" STREQUAL "")
            message(FATAL_ERROR
                "WINK_APP_SOURCES not exported from ${WINK_APP_DIR}. "
                "Its CMakeLists.txt must call set(WINK_APP_SOURCES ... PARENT_SCOPE) "
                "before any EMSCRIPTEN return().")
        endif()

        add_executable(wink_simulator ${WINK_APP_SOURCES})
        target_link_libraries(wink_simulator PRIVATE wink_micro_os)
        target_include_directories(wink_simulator PRIVATE
            ${WINK_APP_DIR}
            ${WINK_APP_INCLUDE_DIRS}
            ${CMAKE_CURRENT_SOURCE_DIR}/dal/include
            ${CMAKE_CURRENT_SOURCE_DIR}/dal/include/input
            ${CMAKE_CURRENT_SOURCE_DIR}/dal/include/output
            ${CMAKE_CURRENT_SOURCE_DIR}/dal/include/actuator
            ${CMAKE_CURRENT_SOURCE_DIR}/dal/include/display
            ${CMAKE_CURRENT_SOURCE_DIR}/dal/include/sensor
            ${WINK_CONFIG_DIR})
    else()
    add_subdirectory(targets/wasm)

    add_subdirectory(${WINK_APP_DIR} ${CMAKE_BINARY_DIR}/app_build)

    if(NOT DEFINED WINK_APP_SOURCES OR "${WINK_APP_SOURCES}" STREQUAL "")
        message(FATAL_ERROR
            "WINK_APP_SOURCES not exported from ${WINK_APP_DIR}. "
            "Its CMakeLists.txt must call set(WINK_APP_SOURCES ... PARENT_SCOPE) "
            "before any EMSCRIPTEN return(). See samples/avoidance_car/CMakeLists.txt.")
    endif()

    set(_WASM_EXPORT_JSON "${CMAKE_CURRENT_SOURCE_DIR}/targets/wasm/exported_runtime_functions.json")
    set(_WASM_EXPORT_CMAKE "${CMAKE_BINARY_DIR}/wasm_export_options.cmake")
    set(_WINK_ENTRY_SCRIPT "${WINK_TOOLS_ROOT}/wink.py")
    if(NOT EXISTS "${_WINK_ENTRY_SCRIPT}")
        set(_WINK_ENTRY_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/wink.py")
    endif()
    execute_process(
        COMMAND ${Python3_EXECUTABLE} "${_WINK_ENTRY_SCRIPT}" --skip-toolchain-check gen wasm-export
                --input "${_WASM_EXPORT_JSON}" --output "${_WASM_EXPORT_CMAKE}"
        RESULT_VARIABLE _wasm_json_rc)
    if(NOT _wasm_json_rc EQUAL 0)
        message(FATAL_ERROR "Failed to parse ${_WASM_EXPORT_JSON}")
    endif()
    include(${_WASM_EXPORT_CMAKE})

    add_executable(wink_simulator
        ${PAL_WASM_SOURCES}
        ${WASM_ENTRY_SOURCE}
        ${WINK_OSAL_SOURCES}
        ${WINK_APP_SOURCES}
        $<TARGET_OBJECTS:pal_common>)

    set(WINK_STRICT_NONBLOCKING 1 CACHE STRING "Enable strict nonblocking for app sources")
    if(WINK_STRICT_NONBLOCKING)
        set_source_files_properties(${WINK_APP_SOURCES} PROPERTIES
            COMPILE_DEFINITIONS WINK_STRICT_NONBLOCKING=1)
    endif()

    target_compile_definitions(wink_simulator PRIVATE WINK_PT_DEBUG)
    target_link_libraries(wink_simulator PRIVATE pal dal wink_runtime wink_bal)

    set(_NEED_ARDUINO FALSE)
    if(WINK_ENABLE_ARDUINO)
        set(_NEED_ARDUINO TRUE)
    else()
        foreach(_src IN LISTS WINK_APP_SOURCES)
            if(_src MATCHES "\\.ino$")
                set(_NEED_ARDUINO TRUE)
                break()
            endif()
        endforeach()
        if(NOT _NEED_ARDUINO AND DEFINED WINK_APP_DIR AND EXISTS "${WINK_APP_DIR}")
            file(GLOB_RECURSE _app_files
                "${WINK_APP_DIR}/*.c" "${WINK_APP_DIR}/*.cpp"
                "${WINK_APP_DIR}/*.h" "${WINK_APP_DIR}/*.hpp"
                "${WINK_APP_DIR}/*.ino")
            foreach(_f IN LISTS _app_files)
                file(READ "${_f}" _f_content LIMIT 2048)
                if(_f_content MATCHES "#include[ \t]*[\"<]Arduino\\.h[\">]")
                    set(_NEED_ARDUINO TRUE)
                    break()
                endif()
            endforeach()
        endif()
    endif()

    if(_NEED_ARDUINO AND TARGET wink_arduino_compat)
        message(STATUS "[frameworks] Arduino compatibility enabled for App")
        target_link_libraries(wink_simulator PRIVATE wink_arduino_compat)
    endif()
    target_include_directories(wink_simulator PRIVATE
        ${WASM_EXTRA_INCLUDE_DIRS}
        ${WINK_OSAL_INCLUDE_DIRS}
        ${WINK_APP_DIR}
        ${WINK_APP_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/dal/include
        ${CMAKE_CURRENT_SOURCE_DIR}/dal/include/input
        ${CMAKE_CURRENT_SOURCE_DIR}/dal/include/output
        ${CMAKE_CURRENT_SOURCE_DIR}/dal/include/actuator
        ${CMAKE_CURRENT_SOURCE_DIR}/dal/include/display
        ${CMAKE_CURRENT_SOURCE_DIR}/dal/include/sensor
        ${WINK_CONFIG_DIR})
    target_link_options(wink_simulator PRIVATE
        "-sERROR_ON_UNDEFINED_SYMBOLS=0"
        "--js-library=${CMAKE_CURRENT_SOURCE_DIR}/targets/wasm/wink_sim_js.js"
        "-sASYNCIFY=1"
        "-sASYNCIFY_IMPORTS=[${WASM_ASYNCIFY_IMPORTS}]"
        "-sASYNCIFY_STACK_SIZE=${WASM_ASYNCIFY_STACK_SIZE}"
        "-sEXPORTED_FUNCTIONS=[${WASM_EXPORT_FUNCTIONS}]"
        "-sEXPORTED_RUNTIME_METHODS=[${WASM_EXPORT_RUNTIME}]"
        "-sMODULARIZE=1"
        "-sEXPORT_NAME=${WASM_EXPORT_NAME}"
        "-sWASM_BIGINT=1"
        "-sSTACK_OVERFLOW_CHECK=2"
        "-sASSERTIONS=1")

    set_property(TARGET wink_simulator APPEND PROPERTY LINK_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/targets/wasm/wink_sim_js.js
        ${CMAKE_CURRENT_SOURCE_DIR}/targets/wasm/exported_runtime_functions.json)

    get_target_property(_wink_simulator_link_libs wink_simulator LINK_LIBRARIES)
    if(NOT "wink_runtime" IN_LIST _wink_simulator_link_libs)
        message(FATAL_ERROR
            "wink_simulator must link wink_runtime; wasm_entry.c depends on "
            "wink_runtime_run().")
    endif()
    endif()
endif()

# Non-WASM (Host/Hardware) targets
if(NOT TARGET_PLATFORM STREQUAL "wasm")
    enable_testing()

    if(WINK_SDK_MODE STREQUAL "binary")
        message(STATUS "BINARY mode: skipping targets/host/ (pal_host from precompiled .a)")
    else()
        add_subdirectory(targets/host)
    endif()

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../wink-micro-app/common)
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../wink-micro-app/common ${CMAKE_BINARY_DIR}/common_build)
    elseif(EXISTS ${WINK_APP_DIR}/../common)
        add_subdirectory(${WINK_APP_DIR}/../common ${CMAKE_BINARY_DIR}/common_build)
    elseif(EXISTS ${WINK_APP_DIR}/common)
        add_subdirectory(${WINK_APP_DIR}/common ${CMAKE_BINARY_DIR}/common_build)
    endif()

    if(EXISTS ${WINK_APP_DIR})
        add_subdirectory(${WINK_APP_DIR} ${CMAKE_BINARY_DIR}/app_build)
    else()
        message(FATAL_ERROR "WINK_APP_DIR does not exist: ${WINK_APP_DIR}")
    endif()

    if(NOT WINK_SDK_BINARY_MODE)
        macro(wink_add_sample_subdirectory name)
            get_filename_component(_abs_sample_dir "${CMAKE_CURRENT_SOURCE_DIR}/../wink-micro-app/${name}" ABSOLUTE)
            if(EXISTS "${_abs_sample_dir}")
                if(NOT "${WINK_APP_DIR}" STREQUAL "${_abs_sample_dir}")
                    add_subdirectory("${_abs_sample_dir}" "${CMAKE_BINARY_DIR}/${name}_build")
                endif()
            endif()
        endmacro()

        wink_add_sample_subdirectory(oled_dashboard)
        wink_add_sample_subdirectory(devkitc_smoke)
        wink_add_sample_subdirectory(resource_conflict)
        wink_add_sample_subdirectory(dual_task_demo)
        wink_add_sample_subdirectory(arduino_blink_demo)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt")
        add_subdirectory(test)
    else()
        message(STATUS "Skipping test/: not present in this SDK tree (Source SDK / M2)")
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/targets/wasm/wasm_node_smoke.cmake")
        include(targets/wasm/wasm_node_smoke.cmake)
    endif()
endif()
