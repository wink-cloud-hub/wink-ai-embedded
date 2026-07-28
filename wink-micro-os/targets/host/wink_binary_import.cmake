# wink_binary_import.cmake — Phase 2 BINARY mode: import precompiled host SDK
#
# Included by the top-level CMakeLists.txt when WINK_SDK_MODE=binary.
# Creates an IMPORTED static library + backward-compatible INTERFACE aliases
# so that existing App CMakeLists (target_link_libraries(... dal), etc.) work
# without modification.

# ── Precompiled static library ──────────────────────────────────────────────
add_library(wink_micro_os STATIC IMPORTED GLOBAL)
set_target_properties(wink_micro_os PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/libs/host/release/libwink_micro_os.a"
)

# Public headers (aggregated by pack_sdk_binary.py)
# DAL and BAL headers live in category subdirectories (input/, output/, etc.)
# but consumer code uses #include "dal_button.h" without prefix — so we add
# each subdirectory to the include search path.
target_include_directories(wink_micro_os INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/hal"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/osal"
    # DAL categories
    "${CMAKE_CURRENT_SOURCE_DIR}/include/input"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/output"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/actuator"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/display"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/sensor"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/storage"
    # DAL/BAL share category subdirs in aggregated include/ (incl. comm/)
    "${CMAKE_CURRENT_SOURCE_DIR}/include/comm"
)

if(MSVC)
    target_link_options(wink_micro_os INTERFACE /OPT:REF)
else()
    target_link_options(wink_micro_os INTERFACE "LINKER:--gc-sections")
endif()

# Consumer-generated wink_config.h (from their own wink-app.json)
if(DEFINED WINK_CONFIG_DIR)
    target_include_directories(wink_micro_os INTERFACE "${WINK_CONFIG_DIR}")
endif()

# test/stubs — host PAL compiles against host_test_ctrl.h; consumer App e2e
# tests may also need the stubs.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/stubs")
    target_include_directories(wink_micro_os INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/test/stubs"
    )
endif()

# ── Backward-compatible INTERFACE aliases ────────────────────────────────────
# Existing App/test CMakeLists link against `dal`, `wink_runtime`, `wink_bal`,
# `pal`, etc.  In BINARY mode all of these resolve to the single merged archive.
foreach(_lib dal wink_runtime wink_trace wink_bal pal)
    if(NOT TARGET ${_lib})
        add_library(${_lib} INTERFACE IMPORTED GLOBAL)
        target_link_libraries(${_lib} INTERFACE wink_micro_os)
    endif()
endforeach()

# pal_host: in SOURCE mode this is an OBJECT library (targets/host/CMakeLists.txt).
# In BINARY mode its objects are already inside libwink_micro_os.a, so we provide
# an INTERFACE stub.  App CMakeLists that use $<TARGET_OBJECTS:pal_host> must
# guard on its existence (see sample_common.cmake WINK_SDK_BINARY_MODE handling).
if(NOT TARGET pal_host)
    add_library(pal_host INTERFACE IMPORTED GLOBAL)
    target_link_libraries(pal_host INTERFACE wink_micro_os)
endif()

# Signal to sample_common.cmake and App CMakeLists that we are in BINARY mode.
set(WINK_SDK_BINARY_MODE TRUE CACHE INTERNAL "Binary SDK mode active")
