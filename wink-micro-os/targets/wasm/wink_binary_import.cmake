# wink_binary_import.cmake — Phase 2 BINARY mode: import precompiled Wasm SDK
#
# Included by the top-level CMakeLists.txt when WINK_SDK_MODE=binary
# and TARGET_PLATFORM=wasm.  Creates an IMPORTED static library + backward-
# compatible INTERFACE aliases so existing App CMakeLists work unchanged.

# ── Precompiled static library ──────────────────────────────────────────────
add_library(wink_micro_os STATIC IMPORTED GLOBAL)
set_target_properties(wink_micro_os PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/libs/wasm/release/libwink_micro_os.a"
)

# Public headers (same whitelist as host — packed by pack_sdk_binary.py)
target_include_directories(wink_micro_os INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/hal"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/osal"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/input"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/output"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/actuator"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/display"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/sensor"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/communication"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/storage"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/comm"
)

# Consumer-generated wink_config.h
if(DEFINED WINK_CONFIG_DIR)
    target_include_directories(wink_micro_os INTERFACE "${WINK_CONFIG_DIR}")
endif()

# ── Wasm link options (from exported_runtime_functions.json SSOT) ───────────
set(_WASM_EXPORT_JSON "${CMAKE_CURRENT_SOURCE_DIR}/targets/wasm/exported_runtime_functions.json")
if(EXISTS "${_WASM_EXPORT_JSON}")
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(_WASM_EXPORT_CMAKE "${CMAKE_BINARY_DIR}/wasm_binary_export_options.cmake")
    if(EXISTS "${WINK_TOOLS_ROOT}/tools/wasm_export_codegen.py")
        set(_WASM_EXPORT_SCRIPT "${WINK_TOOLS_ROOT}/tools/wasm_export_codegen.py")
    else()
        set(_WASM_EXPORT_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/tools/wasm_export_codegen.py")
    endif()
    if(NOT EXISTS "${_WASM_EXPORT_SCRIPT}")
        message(FATAL_ERROR
            "Wasm export codegen script not found: ${_WASM_EXPORT_SCRIPT}. "
            "Re-pack the Binary SDK tarball (tools/wasm_export_codegen.py is required).")
    endif()
    execute_process(
        COMMAND ${Python3_EXECUTABLE} "${_WASM_EXPORT_SCRIPT}"
                "${_WASM_EXPORT_JSON}" "${_WASM_EXPORT_CMAKE}"
        RESULT_VARIABLE _wasm_json_rc)
    if(NOT _wasm_json_rc EQUAL 0)
        message(FATAL_ERROR "Failed to parse ${_WASM_EXPORT_JSON}")
    endif()
    include(${_WASM_EXPORT_CMAKE})
else()
    message(FATAL_ERROR
        "Wasm Binary SDK missing export manifest: ${_WASM_EXPORT_JSON}. "
        "The Binary SDK tarball is incomplete.")
endif()

target_link_options(wink_micro_os INTERFACE
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
    "-sASSERTIONS=1"
)

# ── Backward-compatible INTERFACE aliases ────────────────────────────────────
foreach(_lib dal wink_runtime wink_trace wink_bal pal)
    if(NOT TARGET ${_lib})
        add_library(${_lib} INTERFACE IMPORTED GLOBAL)
        target_link_libraries(${_lib} INTERFACE wink_micro_os)
    endif()
endforeach()

# pal_common: in SOURCE mode this is an OBJECT library; in BINARY mode its
# objects are already inside libwink_micro_os.a.
if(NOT TARGET pal_common)
    add_library(pal_common INTERFACE IMPORTED GLOBAL)
    target_link_libraries(pal_common INTERFACE wink_micro_os)
endif()

set(WINK_SDK_BINARY_MODE TRUE CACHE INTERNAL "Binary SDK mode active")
