# add_wink_wasm_adc_test.cmake — focused emcc+node test for pal_wasm_adc.c.
#
# Included from test/CMakeLists.txt (host build). Compiles a minimal wasm
# artifact containing pal_wasm_adc.c + the target-agnostic physics library +
# Unity + the test TU + tiny link stubs, then runs it under node.
#
# Skips gracefully when emcc or node is missing — plain-C developers without
# the JS/emsdk toolchain still get host-only tests.
#
# This is the first wiring of the long-deferred `add_wink_wasm_test` pattern
# (see test/wasm/README.md §Wiring dependencies). Future wasm-only tests can
# either generalize this helper or copy the structure.

find_program(EMCC_EXECUTABLE emcc)
find_program(NODE_EXECUTABLE node)

if(NOT EMCC_EXECUTABLE)
    message(STATUS "wasm_adc_test: skipping (emcc not found on PATH)")
    return()
endif()
if(NOT NODE_EXECUTABLE)
    message(STATUS "wasm_adc_test: skipping (node not found on PATH)")
    return()
endif()

set(_WASM_ADC_TEST_DIR "${CMAKE_BINARY_DIR}/wasm-adc-test")
file(MAKE_DIRECTORY "${_WASM_ADC_TEST_DIR}")

# helper lives at test/wasm/pal_adc/ ; SDK root is three levels up.
set(_WASM_ADC_HELPER_DIR "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(_SDK_ROOT "${_WASM_ADC_HELPER_DIR}/../../.." ABSOLUTE)

set(_WASM_ADC_TEST_JS "${_WASM_ADC_TEST_DIR}/test_pal_adc_wasm.js")

set(_WASM_ADC_INCLUDES
    -I${_SDK_ROOT}/test/unity
    -I${_SDK_ROOT}/test
    -I${_SDK_ROOT}/pal/include
    -I${_SDK_ROOT}/pal/include/hal
    -I${_SDK_ROOT}/pal/include/osal
    -I${_SDK_ROOT}/pal/include/internal
    -I${_SDK_ROOT}/targets/wasm
    -I${_SDK_ROOT}/targets/common/include
    -I${_SDK_ROOT}/trace/include
)

set(_WASM_ADC_SOURCES
    ${_WASM_ADC_HELPER_DIR}/test_pal_adc_wasm.c
    ${_WASM_ADC_HELPER_DIR}/adc_wasm_link_stubs.c
    ${_SDK_ROOT}/targets/wasm/pal_wasm_adc.c
    ${_SDK_ROOT}/targets/common/src/wink_sim_physical.c
    ${_SDK_ROOT}/targets/common/src/pal_resource.c
    ${_SDK_ROOT}/test/unity/unity.c
)

add_custom_command(
    OUTPUT ${_WASM_ADC_TEST_JS}
    COMMAND ${EMCC_EXECUTABLE}
        ${_WASM_ADC_INCLUDES}
        ${_WASM_ADC_SOURCES}
        -D__EMSCRIPTEN__
        -std=c11
        -O0
        -Wall
        -Wextra
        -Wno-unused-parameter
        -Wno-missing-field-initializers
        -Wno-error=unused-but-set-variable
        -Wno-error=deprecated-declarations
        -sWASM=1
        -sEXIT_RUNTIME=1
        -sALLOW_MEMORY_GROWTH=0
        -sERROR_ON_UNDEFINED_SYMBOLS=1
        -sEXPORTED_RUNTIME_METHODS=['UTF8ToString']
        -ffunction-sections -fdata-sections
        -Wl,--gc-sections
        -o ${_WASM_ADC_TEST_JS}
    DEPENDS
        ${_WASM_ADC_SOURCES}
    COMMENT "Building wasm ADC unit test (emcc)"
    VERBATIM
)

add_custom_target(wasm_adc_test_build ALL DEPENDS ${_WASM_ADC_TEST_JS})

add_test(
    NAME wasm_adc_test
    COMMAND ${NODE_EXECUTABLE} ${_WASM_ADC_TEST_JS}
)
set_tests_properties(wasm_adc_test PROPERTIES
    TIMEOUT 60
    LABELS  "wasm"
    DEPENDS wasm_adc_test_build
)
message(STATUS "wasm_adc_test: registered (emcc=${EMCC_EXECUTABLE}, node=${NODE_EXECUTABLE})")
