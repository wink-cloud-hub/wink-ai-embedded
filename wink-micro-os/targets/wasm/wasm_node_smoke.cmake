# wasm_node_smoke.cmake
#
# Host-build helper: builds the unisim_smoke wasm variant via emcmake (out-of-source
# ExternalProject) and registers a ctest entry that runs `node wink_sim_stub.js`
# against it. Result: a plain `ctest` in a host build dir now covers the wasm
# toolchain path (instantiation + Asyncify + cooperative scheduler + virtual-clock
# advancement) in addition to the host Unity tests.
#
# Gracefully skips when `node` or `emcmake` is not on PATH (no hard failure — plain-C
# developers without the JS/emsdk toolchain still get host-only tests).
#
# Included from the top-level CMakeLists.txt ONLY in the host-build branch (i.e.
# TARGET_PLATFORM != wasm) after enable_testing().

include(ExternalProject)

find_program(NODE_EXECUTABLE node)
find_program(EMCMAKE_EXECUTABLE emcmake)

if(NOT NODE_EXECUTABLE)
    message(STATUS "wasm_node_smoke: skipping (node not found on PATH)")
    return()
endif()
if(NOT EMCMAKE_EXECUTABLE)
    message(STATUS "wasm_node_smoke: skipping (emcmake not found on PATH; install / activate emsdk)")
    return()
endif()

set(WASM_SMOKE_SOURCE_DIR  "${CMAKE_CURRENT_SOURCE_DIR}")
set(WASM_SMOKE_BINARY_DIR  "${CMAKE_BINARY_DIR}/wasm-unisim-smoke")
set(WASM_SMOKE_STUB_JS     "${CMAKE_CURRENT_SOURCE_DIR}/targets/wasm/wink_sim_stub.js")

# Override CMAKE_COMMAND to wrap cmake with emcmake. ExternalProject_Add
# constructs the configure command as ${CMAKE_COMMAND} -S <src> -B <bin> ${CMAKE_ARGS},
# so this expands to `emcmake cmake -S <src> -B <bin> -DTARGET_PLATFORM=wasm ...`
# which is exactly the documented invocation (see wink_sim_stub.js header comment).
# BUILD_COMMAND is left at its default (`${CMAKE_COMMAND} --build <bin>`), which is
# plain cmake (emcmake only needs to wrap configure).
ExternalProject_Add(wasm_unisim_smoke_build
    SOURCE_DIR       "${WASM_SMOKE_SOURCE_DIR}"
    BINARY_DIR       "${WASM_SMOKE_BINARY_DIR}"
    CMAKE_COMMAND    "${EMCMAKE_EXECUTABLE}" "${CMAKE_COMMAND}"
    CMAKE_ARGS
        -DTARGET_PLATFORM=wasm
        -DWINK_APP_DIR=samples/unisim_smoke
        -DCMAKE_BUILD_TYPE=Debug
    INSTALL_COMMAND  ""
    TEST_COMMAND     ""
    BUILD_ALWAYS     ON
    EXCLUDE_FROM_ALL ON
    STEP_TARGETS     build
)

add_test(
    NAME wasm_node_smoke
    COMMAND "${NODE_EXECUTABLE}" "${WASM_SMOKE_STUB_JS}"
            --build-dir=${WASM_SMOKE_BINARY_DIR}
)

set_tests_properties(wasm_node_smoke PROPERTIES
    TIMEOUT 120
    LABELS  "wasm"
    DEPENDS wasm_unisim_smoke_build-build
)

message(STATUS "wasm_node_smoke: registered ctest 'wasm_node_smoke' (node=${NODE_EXECUTABLE}, emcmake=${EMCMAKE_EXECUTABLE})")
