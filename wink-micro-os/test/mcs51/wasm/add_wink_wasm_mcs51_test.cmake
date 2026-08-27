# add_wink_wasm_mcs51_test.cmake — emcc+Node test for the MCS-51 interception
# layer (Axis B, ADR-0070).
#
# Included from test/CMakeLists.txt (host build). Runs the UNMODIFIED Keil
# blinky sample through the cleanup pass (blinky.c -> blinky.cpp copy in the
# build tree; the source is never edited in place), compiles it together with
# the mcs51 compat framework + cooperative fiber runtime under emcc with
# ASYNCIFY fibers, and executes under node.
#
# The channel data-plane PALs (pal_wasm_ch*.c) are deliberately omitted — they
# drag in the full GPIO/I2C/UART/ADC js_ data plane; pal_wasm_degradation.c's
# channel *_reset() hooks are satisfied by mcs51_wasm_link_stubs.c no-ops.
#
# Skips gracefully when emcc or node is missing — plain-C developers without
# the JS/emsdk toolchain still get host-only tests. Mirrors
# test/wasm/pal_adc/add_wink_wasm_adc_test.cmake.

find_program(EMCC_EXECUTABLE emcc)
find_program(NODE_EXECUTABLE node)

if(NOT EMCC_EXECUTABLE)
    message(STATUS "wasm_mcs51_test: skipping (emcc not found on PATH)")
    return()
endif()
if(NOT NODE_EXECUTABLE)
    message(STATUS "wasm_mcs51_test: skipping (node not found on PATH)")
    return()
endif()

# helper lives at test/mcs51/wasm/ ; SDK root is four levels up.
set(_WASM_MCS51_HELPER_DIR "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(_SDK_ROOT "${_WASM_MCS51_HELPER_DIR}/../../.." ABSOLUTE)

set(_WASM_MCS51_DIR "${CMAKE_BINARY_DIR}/wasm-mcs51-test")
file(MAKE_DIRECTORY "${_WASM_MCS51_DIR}/gen")

set(_MCS51_BLINKY_CPP "${_WASM_MCS51_DIR}/blinky.cpp")
set(_MCS51_CONFIG_H  "${_WASM_MCS51_DIR}/gen/wink_config.h")
set(_WASM_MCS51_JS   "${_WASM_MCS51_DIR}/test_mcs51_blinky_wasm.js")

# ── Step 1: Keil dialect cleanup (blinky.c -> blinky.cpp) ───────────────────
add_custom_command(
    OUTPUT ${_MCS51_BLINKY_CPP}
    COMMAND ${Python3_EXECUTABLE}
        ${_SDK_ROOT}/frameworks/mcs51/tools/mcs51_cleanup.py
        ${_SDK_ROOT}/test/mcs51/samples/blinky.c
        ${_MCS51_BLINKY_CPP}
    DEPENDS ${_SDK_ROOT}/test/mcs51/samples/blinky.c
            ${_SDK_ROOT}/frameworks/mcs51/tools/mcs51_cleanup.py
    COMMENT "mcs51 wasm cleanup: blinky.c -> blinky.cpp"
    VERBATIM)

# ── Step 2: generate wink_config.h for the wasm target ──────────────────────
# DAL/PAL headers include wink_config.h; generate a wasm-targeted copy into
# the test build dir (same generator the top-level generate_config target uses).
set(_MCS51_CONFIG_H_DEPS)
if(WINK_TOOLS_ROOT AND EXISTS "${WINK_TOOLS_ROOT}/tools/codegen/generators/config_h.py")
    list(APPEND _MCS51_CONFIG_H_DEPS
        "${WINK_TOOLS_ROOT}/tools/codegen/generators/config_h.py")
endif()
add_custom_command(
    OUTPUT ${_MCS51_CONFIG_H}
    COMMAND ${Python3_EXECUTABLE}
        "${WINK_TOOLS_ROOT}/tools/codegen/generators/config_h.py"
        --input "${WINK_APP_JSON}"
        --output ${_MCS51_CONFIG_H}
        --target wasm
    DEPENDS ${_MCS51_CONFIG_H_DEPS}
    COMMENT "mcs51 wasm: generating wink_config.h"
    VERBATIM)

# ── Step 3: emcc compile + link ─────────────────────────────────────────────
set(_WASM_MCS51_INCLUDES
    -I${_SDK_ROOT}/frameworks/mcs51/include
    -I${_SDK_ROOT}/pal/include
    -I${_SDK_ROOT}/pal/include/osal
    -I${_SDK_ROOT}/pal/include/hal
    -I${_SDK_ROOT}/pal/include/internal
    -I${_SDK_ROOT}/runtime/include
    -I${_SDK_ROOT}/trace/include
    -I${_SDK_ROOT}/targets/common/include
    -I${_SDK_ROOT}/targets/wasm
    -I${_SDK_ROOT}/dal/include
    -I${_SDK_ROOT}/dal/include/input
    -I${_SDK_ROOT}/dal/include/output
    -I${_SDK_ROOT}/dal/include/actuator
    -I${_SDK_ROOT}/dal/include/display
    -I${_SDK_ROOT}/dal/include/sensor
    -I${_SDK_ROOT}/dal/include/comm
    -I${_SDK_ROOT}/dal/include/storage
    -I${_SDK_ROOT}/bal/include
    -I${_WASM_MCS51_DIR}/gen
)

# Non-channel wasm PALs (channels pull the js_ data plane; omitted + stubbed).
file(GLOB _WASM_MCS51_PAL_WASM
    "${_SDK_ROOT}/targets/wasm/pal_wasm_*.c")
list(FILTER _WASM_MCS51_PAL_WASM EXCLUDE REGEX "_ch[0-9]")

set(_WASM_MCS51_SOURCES
    ${_SDK_ROOT}/test/mcs51/wasm/test_mcs51_blinky_wasm.c
    ${_MCS51_BLINKY_CPP}
    ${_SDK_ROOT}/frameworks/mcs51/src/mcs51_proxy.cpp
    ${_SDK_ROOT}/frameworks/mcs51/src/mcs51_isr.cpp
    ${_SDK_ROOT}/frameworks/mcs51/src/mcs51_bridge.cpp
    ${_SDK_ROOT}/runtime/src/wink_runtime.c
    ${_SDK_ROOT}/runtime/src/wink_runtime_tasks.c
    ${_SDK_ROOT}/runtime/src/wink_actuator_registry.c
    ${_SDK_ROOT}/runtime/src/wink_soft_timer.c
    ${_SDK_ROOT}/runtime/src/wink_event.c
    ${_SDK_ROOT}/runtime/src/wink_dev_config.c
    ${_SDK_ROOT}/trace/src/wink_trace.c
    ${_SDK_ROOT}/targets/common/src/wink_sim_scheduler.c
    ${_SDK_ROOT}/targets/common/src/wink_sim_physical.c
    ${_SDK_ROOT}/targets/common/src/pal_resource.c
    ${_SDK_ROOT}/osal/wasm/pal_osal_wasm.c
    ${_SDK_ROOT}/osal/wasm/sim_ctx_emscripten_fiber.c
    ${_SDK_ROOT}/osal/wasm/pal_deferred_wasm.c
    ${_SDK_ROOT}/osal/common/pal_osal_ringbuf.c
    ${_WASM_MCS51_PAL_WASM}
    ${_SDK_ROOT}/targets/wasm/pal_log_wasm.c
    ${_SDK_ROOT}/test/mcs51/wasm/mcs51_wasm_link_stubs.c
)

add_custom_command(
    OUTPUT ${_WASM_MCS51_JS}
    COMMAND ${EMCC_EXECUTABLE}
        ${_WASM_MCS51_INCLUDES}
        ${_WASM_MCS51_SOURCES}
        -O1
        -DSIMULATION=1
        -DPLATFORM_wasm
        -Wno-write-strings
        -Wno-deprecated-declarations
        -sASYNCIFY=1
        -sASYNCIFY_STACK_SIZE=65536
        -sENVIRONMENT=node
        -sALLOW_MEMORY_GROWTH=1
        -sERROR_ON_UNDEFINED_SYMBOLS=1
        -sEXIT_RUNTIME=1
        --js-library=${_SDK_ROOT}/test/mcs51/wasm/mcs51_wasm_node_stub.js
        -o ${_WASM_MCS51_JS}
    DEPENDS
        ${_WASM_MCS51_SOURCES}
        ${_MCS51_BLINKY_CPP}
        ${_MCS51_CONFIG_H}
        ${_SDK_ROOT}/test/mcs51/wasm/mcs51_wasm_node_stub.js
    COMMENT "Building mcs51 blinky wasm test (emcc + ASYNCIFY fibers)"
    VERBATIM)

add_custom_target(wasm_mcs51_test_build ALL DEPENDS ${_WASM_MCS51_JS})

add_test(
    NAME wasm_mcs51_test
    COMMAND ${NODE_EXECUTABLE} ${_WASM_MCS51_JS}
)
set_tests_properties(wasm_mcs51_test PROPERTIES
    TIMEOUT 120
    LABELS  "wasm"
)
message(STATUS "wasm_mcs51_test: registered (emcc=${EMCC_EXECUTABLE}, node=${NODE_EXECUTABLE})")
