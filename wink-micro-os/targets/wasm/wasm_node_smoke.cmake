# wasm_node_smoke.cmake
#
# Host-build helper: builds the unisim_smoke wasm variant via emcmake (out-of-source
# ExternalProject) and registers a ctest entry that runs `node wink_sim_stub.js`
# against it.

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

set(_WASM_SMOKE_HOST_CMAKE "${CMAKE_COMMAND}")

find_program(NINJA_EXECUTABLE ninja)
find_program(MINGW_MAKE_EXECUTABLE NAMES mingw32-make)
find_program(UNIX_MAKE_EXECUTABLE NAMES gmake make)

if(NINJA_EXECUTABLE)
    set(_WASM_SMOKE_GENERATOR "Ninja")
    set(_WASM_SMOKE_MAKE_PROGRAM "${NINJA_EXECUTABLE}")
    set(_WASM_SMOKE_BUILD_TOOL "${NINJA_EXECUTABLE}")
    set(_WASM_SMOKE_BUILD_HAS_PARALLEL_ARG FALSE)
elseif(WIN32 AND MINGW_MAKE_EXECUTABLE)
    set(_WASM_SMOKE_GENERATOR "MinGW Makefiles")
    set(_WASM_SMOKE_MAKE_PROGRAM "${MINGW_MAKE_EXECUTABLE}")
    set(_WASM_SMOKE_BUILD_TOOL "${MINGW_MAKE_EXECUTABLE}")
    set(_WASM_SMOKE_BUILD_HAS_PARALLEL_ARG TRUE)
elseif(NOT WIN32 AND UNIX_MAKE_EXECUTABLE)
    set(_WASM_SMOKE_GENERATOR "Unix Makefiles")
    set(_WASM_SMOKE_MAKE_PROGRAM "${UNIX_MAKE_EXECUTABLE}")
    set(_WASM_SMOKE_BUILD_TOOL "${UNIX_MAKE_EXECUTABLE}")
    set(_WASM_SMOKE_BUILD_HAS_PARALLEL_ARG TRUE)
else()
    message(STATUS "wasm_node_smoke: skipping (need ninja or make for inner emcmake build)")
    return()
endif()
message(STATUS "wasm_node_smoke: inner generator = ${_WASM_SMOKE_GENERATOR}")

set(_WASM_SMOKE_JS "${WASM_SMOKE_BINARY_DIR}/wink_simulator.js")
if(_WASM_SMOKE_BUILD_HAS_PARALLEL_ARG)
    set(_WASM_SMOKE_BUILD_CMD
        "${CMAKE_COMMAND}" -E chdir "${WASM_SMOKE_BINARY_DIR}"
        "${_WASM_SMOKE_BUILD_TOOL}" -j)
else()
    set(_WASM_SMOKE_BUILD_CMD
        "${CMAKE_COMMAND}" -E chdir "${WASM_SMOKE_BINARY_DIR}"
        "${_WASM_SMOKE_BUILD_TOOL}")
endif()

# Workspace layouts where the toolchain does not sit at ../wink-tools or
# ../packages/wink-tools cannot rely on the inner project's path probing:
# forward an explicitly configured WINK_TOOLS_ROOT into the external project.
set(_WASM_SMOKE_EXTRA_ARGS "")
if(DEFINED WINK_TOOLS_ROOT AND NOT WINK_TOOLS_ROOT STREQUAL "")
    list(APPEND _WASM_SMOKE_EXTRA_ARGS "-DWINK_TOOLS_ROOT=${WINK_TOOLS_ROOT}")
endif()

ExternalProject_Add(wasm_unisim_smoke_build
    SOURCE_DIR          "${WASM_SMOKE_SOURCE_DIR}"
    BINARY_DIR          "${WASM_SMOKE_BINARY_DIR}"
    CMAKE_COMMAND       "${EMCMAKE_EXECUTABLE}" "${_WASM_SMOKE_HOST_CMAKE}"
    CMAKE_GENERATOR     "${_WASM_SMOKE_GENERATOR}"
    BUILD_COMMAND       ${_WASM_SMOKE_BUILD_CMD}
    BUILD_BYPRODUCTS    "${_WASM_SMOKE_JS}"
    CMAKE_ARGS
        -DCMAKE_MAKE_PROGRAM=${_WASM_SMOKE_MAKE_PROGRAM}
        -DTARGET_PLATFORM=wasm
        -DWINK_APP_DIR=../wink-micro-app/fixtures/unisim_smoke
        -DCMAKE_BUILD_TYPE=Debug
        # The unisim_smoke fixture is a test shim whose only job is to reach
        # every blocking-only js_* import once (sync I2C transfer, pulse_in,
        # sleep_ms). It is not production app code, so it opts out of the
        # global strict-nonblocking compile definition.
        -DWINK_STRICT_NONBLOCKING=0
        ${_WASM_SMOKE_EXTRA_ARGS}
    INSTALL_COMMAND     ""
    TEST_COMMAND        ""
    BUILD_ALWAYS        ON
    EXCLUDE_FROM_ALL    ON
    STEP_TARGETS        build
)

# The smoke tree is an EXCLUDE_FROM_ALL ExternalProject, so a plain
# `cmake --build` never produces its artifacts, and ctest DEPENDS only orders
# other TESTS (a CMake target name there is a no-op). Build the artifacts in a
# fixture setup test that the smoke test requires: only a `ctest -L wasm` (or
# an explicit -R) pays for the emcc build, and the smoke test can no longer
# run against a missing tree.
add_test(
    NAME wasm_node_smoke_build
    COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}"
            --target wasm_unisim_smoke_build
)
set_tests_properties(wasm_node_smoke_build PROPERTIES
    TIMEOUT 1800
    LABELS  "wasm"
    FIXTURES_SETUP wasm_node_smoke_artifacts
)

add_test(
    NAME wasm_node_smoke
    COMMAND "${NODE_EXECUTABLE}" "${WASM_SMOKE_STUB_JS}"
            --build-dir=${WASM_SMOKE_BINARY_DIR}
)

set_tests_properties(wasm_node_smoke PROPERTIES
    TIMEOUT 120
    LABELS  "wasm"
    FIXTURES_REQUIRED wasm_node_smoke_artifacts
)

message(STATUS "wasm_node_smoke: registered ctest 'wasm_node_smoke' (node=${NODE_EXECUTABLE}, emcmake=${EMCMAKE_EXECUTABLE})")
