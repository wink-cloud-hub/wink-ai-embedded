# ============================================================
# main component for ESP32 firmware builds.
#
# Source files are assembled from three places:
#   1. app_main.c  (shim that calls into wink runtime + app callbacks)
#   2. Auto-scanned App sources + samples/common helpers
#   3. Codegen outputs (device_tree.[ch] + app_support.c)
# ============================================================
if(NOT DEFINED WINK_APP)
    set(WINK_APP "devkitc_smoke" CACHE STRING "Samples app name to build")
endif()

if(DEFINED ENV{WINK_APP_DIR} AND NOT DEFINED WINK_APP_DIR)
    set(WINK_APP_DIR $ENV{WINK_APP_DIR})
endif()

if(CMAKE_BUILD_EARLY_EXPANSION)
    set(_WINK_GEN_PY "python")
else()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(_WINK_GEN_PY "${Python3_EXECUTABLE}")
endif()

if(NOT DEFINED WINK_SDK_PATH AND DEFINED ENV{WINK_SDK_PATH})
    set(WINK_SDK_PATH "$ENV{WINK_SDK_PATH}")
endif()
if(DEFINED WINK_SDK_PATH)
    set(_WINK_GEN_SCRIPT "${WINK_SDK_PATH}/tools/esp32/generate_app_sources.py")
else()
    set(_WINK_GEN_SCRIPT "${COMPONENT_DIR}/../../wink-tools/tools/esp32/generate_app_sources.py")
endif()

if(DEFINED WINK_APP_DIR)
    set(_WINK_GEN_APP_ARGS --app-dir "${WINK_APP_DIR}")
else()
    set(_WINK_GEN_APP_ARGS --app-name "${WINK_APP}")
endif()

execute_process(
    COMMAND "${_WINK_GEN_PY}" "${_WINK_GEN_SCRIPT}"
            --esp32-firmware-dir "${COMPONENT_DIR}/.."
            ${_WINK_GEN_APP_ARGS}
    WORKING_DIRECTORY "${COMPONENT_DIR}/.."
    RESULT_VARIABLE GEN_SCRIPT_RESULT
)

if(NOT GEN_SCRIPT_RESULT EQUAL 0)
    message(FATAL_ERROR "generate_app_sources.py failed with code ${GEN_SCRIPT_RESULT}")
endif()

include(${COMPONENT_DIR}/app_sources.cmake)

set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GEN_DIR})

set(IS_ARDUINO_APP FALSE)
foreach(src ${WINK_APP_SOURCES})
    if(src MATCHES "\\.ino$")
        set(IS_ARDUINO_APP TRUE)
    endif()
endforeach()

set(APP_REQUIRES driver esp32)
if(IS_ARDUINO_APP)
    list(APPEND APP_REQUIRES arduino)
endif()

idf_component_register(
    SRCS
        "app_main.c"
        ${WINK_APP_SOURCES}
    INCLUDE_DIRS
        "."
        ${WINK_APP_DIR}
        ${GEN_DIR}
        ${WINK_APP_COMMON_INCLUDE_DIR}
    REQUIRES ${APP_REQUIRES}
    WHOLE_ARCHIVE
)

foreach(src ${WINK_APP_SOURCES})
    if(src MATCHES "\\.ino$")
        set_source_files_properties(${src} PROPERTIES LANGUAGE CXX)
        if(MSVC)
            set_source_files_properties(${src} PROPERTIES COMPILE_FLAGS "/TP")
        else()
            set_source_files_properties(${src} PROPERTIES COMPILE_FLAGS "-x c++")
        endif()
    endif()
endforeach()

# ── Codegen (wink-app.json → device_tree.[ch]) ────────
find_package(Python3 REQUIRED COMPONENTS Interpreter)

if(NOT DEFINED WINK_CODEGEN_ROOT)
    set(WINK_CODEGEN_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../wink-tools/tools/codegen")
endif()
set(WINK_CODEGEN_TOOL "${WINK_CODEGEN_ROOT}/app_codegen.py")
set(WINK_APP_JSON     "${WINK_APP_DIR}/wink-app.json")

if(EXISTS "${WINK_APP_JSON}")
    set(GEN_DEVICE_TREE_H ${GEN_DIR}/device_tree.h)
    set(GEN_DEVICE_TREE_C ${GEN_DIR}/device_tree.c)
    set(GEN_APP_OPTIONS   ${GEN_DIR}/app_options.cmake)

    file(GLOB WINK_CODEGEN_TEMPLATES ${WINK_CODEGEN_ROOT}/templates/*.j2)
    file(GLOB WINK_CODEGEN_DRIVERS   ${WINK_CODEGEN_ROOT}/drivers/*.py)

    add_custom_command(
        OUTPUT ${GEN_DEVICE_TREE_H} ${GEN_DEVICE_TREE_C}
               ${GEN_APP_OPTIONS}
        COMMAND ${Python3_EXECUTABLE} ${WINK_CODEGEN_TOOL}
                --config ${WINK_APP_JSON}
                --out-dir ${GEN_DIR}
        DEPENDS ${WINK_APP_JSON} ${WINK_CODEGEN_TOOL}
                ${WINK_CODEGEN_TEMPLATES} ${WINK_CODEGEN_DRIVERS}
        COMMENT "Generating ${WINK_APP} device tree from wink-app.json (esp32)"
        VERBATIM
    )

    target_sources(${COMPONENT_LIB} PRIVATE
        ${GEN_DEVICE_TREE_C}
    )

    add_custom_target(wink_app_codegen DEPENDS
        ${GEN_DEVICE_TREE_H} ${GEN_DEVICE_TREE_C})
    add_dependencies(${COMPONENT_LIB} wink_app_codegen)
endif()

target_compile_options(${COMPONENT_LIB} PRIVATE
    -finput-charset=UTF-8
    -fexec-charset=UTF-8
)

if(WINK_APP STREQUAL "devkitc_smoke")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE WINK_CFG_SIM_ECHO=1)
endif()
