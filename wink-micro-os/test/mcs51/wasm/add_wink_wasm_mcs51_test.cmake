# add_wink_wasm_mcs51_test.cmake — emcc+Node tests for the MCS-51
# interception layer (Axis B, ADR-0070; M2 adds the timer0 ISR test).
#
# Included from test/CMakeLists.txt (host build). Runs the UNMODIFIED Keil
# samples through the cleanup pass (<sample>.c -> <sample>.cpp copy in the
# build tree; the source is never edited in place), compiles them together
# with the mcs51 compat framework + cooperative fiber runtime under emcc with
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
    message(STATUS "wasm_mcs51 tests: skipping (emcc not found on PATH)")
    return()
endif()
if(NOT NODE_EXECUTABLE)
    message(STATUS "wasm_mcs51 tests: skipping (node not found on PATH)")
    return()
endif()

# helper lives at test/mcs51/wasm/ ; SDK root is four levels up.
set(_WASM_MCS51_HELPER_DIR "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(_SDK_ROOT "${_WASM_MCS51_HELPER_DIR}/../../.." ABSOLUTE)

set(_WASM_MCS51_DIR "${CMAKE_BINARY_DIR}/wasm-mcs51-test")
file(MAKE_DIRECTORY ${_WASM_MCS51_DIR}/gen)

set(_MCS51_CONFIG_H "${_WASM_MCS51_DIR}/gen/wink_config.h")

# ── generate wink_config.h for the wasm target (shared by both tests) ────────
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

# ── M6: board-config codegen (wink-app.json -> mcs51_board_config.h) ──────────
# The framework bridge __has_include()s this at compile time; the gen dir is
# already in _WASM_MCS51_INCLUDES (-I.../gen). When present the bridge auto-binds
# the codegen ADC0832, so the iron_ntc driver never calls mcs51_adc0832_init.
# Gated on the generator (skips gracefully when wink-tools source is absent).
set(_MCS51_BOARD_CONFIG_H "")
set(_MCS51_BOARD_CONFIG_GENERATOR
    "${WINK_TOOLS_ROOT}/tools/codegen/generators/mcs51_board_config.py")
set(_MCS51_IRON_NTC_APP "${_SDK_ROOT}/test/mcs51/apps/iron_ntc/wink-app.json")
if(WINK_TOOLS_ROOT AND EXISTS "${_MCS51_BOARD_CONFIG_GENERATOR}"
        AND EXISTS "${_MCS51_IRON_NTC_APP}")
    set(_MCS51_BOARD_CONFIG_H "${_WASM_MCS51_DIR}/gen/mcs51_board_config.h")
    add_custom_command(
        OUTPUT ${_MCS51_BOARD_CONFIG_H}
        COMMAND ${Python3_EXECUTABLE} "${_MCS51_BOARD_CONFIG_GENERATOR}"
            --input "${_MCS51_IRON_NTC_APP}"
            --output ${_MCS51_BOARD_CONFIG_H}
        DEPENDS "${_MCS51_IRON_NTC_APP}"
                "${_MCS51_BOARD_CONFIG_GENERATOR}"
                "${WINK_TOOLS_ROOT}/tools/codegen/templates/mcs51_board_config.h.j2"
                "${WINK_TOOLS_ROOT}/tools/codegen/boards/mcs51_devboard.json"
        COMMENT "mcs51 wasm: generating mcs51_board_config.h (iron_ntc)"
        VERBATIM)
else()
    message(STATUS "wasm_mcs51: mcs51_board_config generator/iron_ntc app not "
        "found — skipping wasm_mcs51_iron_ntc_test")
endif()

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

# add_wink_wasm_mcs51_test(<test_name> <sample_name> <driver_c> [extra_emcc_flags]):
#   cleanup <sample_name>.c -> <sample_name>.cpp, compile with the framework
#   under emcc + ASYNCIFY, register a ctest under node. The optional 4th arg is
#   appended to the emcc command line (e.g. -sEXPORTED_FUNCTIONS=... for a test
#   that exports a getter the node JS library calls back into).
function(add_wink_wasm_mcs51_test test_name sample_name driver_c)
    set(_extra_emcc_flags "${ARGN}")
    set(_sample_cpp "${_WASM_MCS51_DIR}/${sample_name}.cpp")
    set(_out_js     "${_WASM_MCS51_DIR}/${test_name}.js")

    # Keil dialect cleanup: <sample>.c -> <sample>.cpp (source never edited).
    add_custom_command(
        OUTPUT ${_sample_cpp}
        COMMAND ${Python3_EXECUTABLE}
            ${_SDK_ROOT}/frameworks/mcs51/tools/mcs51_cleanup.py
            ${_SDK_ROOT}/test/mcs51/samples/${sample_name}.c
            ${_sample_cpp}
        DEPENDS ${_SDK_ROOT}/test/mcs51/samples/${sample_name}.c
                ${_SDK_ROOT}/frameworks/mcs51/tools/mcs51_cleanup.py
        COMMENT "mcs51 wasm cleanup: ${sample_name}.c -> ${sample_name}.cpp"
        VERBATIM)

    set(_test_sources
        ${driver_c}
        ${_sample_cpp}
        ${_SDK_ROOT}/frameworks/mcs51/src/mcs51_sfr.cpp
        ${_SDK_ROOT}/frameworks/mcs51/src/mcs51_adc.cpp
        ${_SDK_ROOT}/frameworks/mcs51/src/mcs51_adc0832.cpp
        ${_SDK_ROOT}/frameworks/mcs51/src/cms8s_adc.cpp
        ${_SDK_ROOT}/frameworks/mcs51/src/mcs51_isr.cpp
        ${_SDK_ROOT}/frameworks/mcs51/src/mcs51_clock.cpp
        ${_SDK_ROOT}/frameworks/mcs51/src/mcs51_timer.cpp
        ${_SDK_ROOT}/frameworks/mcs51/src/mcs51_uart.cpp
        ${_SDK_ROOT}/frameworks/mcs51/src/mcs51_xdata.cpp
        ${_SDK_ROOT}/frameworks/mcs51/src/mcs51_unsupported.cpp
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
        OUTPUT ${_out_js}
        COMMAND ${EMCC_EXECUTABLE}
            ${_WASM_MCS51_INCLUDES}
            ${_test_sources}
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
            ${_extra_emcc_flags}
            -o ${_out_js}
        DEPENDS
            ${_test_sources}
            ${_sample_cpp}
            ${_MCS51_CONFIG_H}
            ${_MCS51_BOARD_CONFIG_H}
            ${_SDK_ROOT}/test/mcs51/wasm/mcs51_wasm_node_stub.js
        COMMENT "Building ${test_name} (emcc + ASYNCIFY fibers)"
        VERBATIM)

    add_custom_target(${test_name}_build ALL DEPENDS ${_out_js})

    add_test(
        NAME ${test_name}
        COMMAND ${NODE_EXECUTABLE} ${_out_js}
    )
    set_tests_properties(${test_name} PROPERTIES
        TIMEOUT 120
        LABELS  "wasm"
    )
    message(STATUS "${test_name}: registered (emcc=${EMCC_EXECUTABLE}, node=${NODE_EXECUTABLE})")
endfunction()

# M1: polling blinky (SFR proxies + cooperative yield, no ISR dispatch).
add_wink_wasm_mcs51_test(
    wasm_mcs51_test
    blinky
    ${_SDK_ROOT}/test/mcs51/wasm/test_mcs51_blinky_wasm.c)

# M2: Timer0 50 ms overflow drives the ISR; tight super-loop does not freeze.
add_wink_wasm_mcs51_test(
    wasm_mcs51_timer0_test
    blinky_timer0
    ${_SDK_ROOT}/test/mcs51/wasm/test_mcs51_timer0_wasm.c)

# M3: UART SBUF write emits bytes to the Node console (stdout) and the C-ABI
# capture buffer; TI is set synchronously so `while(!TI)` closes on first read.
# Stage 2: also asserts the live ch2 route — the Node stub's js_pal_uart_write
# calls back into the exported mcs51_wasm_uart_accept_byte sink.
add_wink_wasm_mcs51_test(
    wasm_mcs51_uart_test
    uart_printf
    ${_SDK_ROOT}/test/mcs51/wasm/test_mcs51_uart_wasm.c
    "-sEXPORTED_FUNCTIONS=_main,_mcs51_wasm_uart_accept_byte")

# Stage 2 T2: UART echo — RX bytes pushed from the post-init hook drain at
# microstep points (RI + vector 4), the ISR stashes them, the main loop echoes
# via SBUF; the echoed sequence returns through the Node stub's js_pal_uart_write
# into the exported accept sink.
add_wink_wasm_mcs51_test(
    wasm_mcs51_uart_echo_test
    uart_echo
    ${_SDK_ROOT}/test/mcs51/test_mcs51_uart_echo_e2e.c
    "-sEXPORTED_FUNCTIONS=_main,_mcs51_wasm_uart_accept_byte")

# M3: GPIO in->out sync — P3.2 key (latch-injected) drives P1.0 LED across
# three repeated runtime runs (released -> pressed -> released) under Node.
add_wink_wasm_mcs51_test(
    wasm_mcs51_gpio_test
    gpio_in_out
    ${_SDK_ROOT}/test/mcs51/wasm/test_mcs51_gpio_wasm.c)

# M4: ADC0832 3-wire DIO end-to-end — the unmodified Keil bit-bang sample reads
# CH0/CH1 through the instant Level-2 trap FSM; the shared host/wasm C driver
# binds the traps via the post-init hook and injects 0xA5/0x5A on channel-3.
add_wink_wasm_mcs51_test(
    wasm_mcs51_adc0832_test
    adc0832_read
    ${_SDK_ROOT}/test/mcs51/test_mcs51_adc0832_e2e.c)

# M5: CMS8S78xx on-chip ADC end-to-end — the unmodified Keil polled sample
# drives the real register map (ADCON0 ADGO/ADFM, ADRESH/ADRESL packing); the
# shared host/wasm C driver injects 0xABC/0x801/0xFFF on AN0/AN1/AN25 via the
# post-init hook and asserts the recombined 12-bit codes.
add_wink_wasm_mcs51_test(
    wasm_mcs51_cms8s_adc_test
    cms8s_adc_test
    ${_SDK_ROOT}/test/mcs51/test_mcs51_cms8s_adc_e2e.c)

# M6: NTC closed-loop thermostat through the board-codegen ADC0832 seam —
# the shared host/wasm C driver injects cold/hot/open/short codes via the
# post-init hook (no mcs51_adc0832_init in the driver: the bridge auto-binds
# the codegen pins) and asserts heater toggle + open/short safe states.
if(_MCS51_BOARD_CONFIG_H)
    add_wink_wasm_mcs51_test(
        wasm_mcs51_iron_ntc_test
        iron_ntc
        ${_SDK_ROOT}/test/mcs51/test_mcs51_iron_ntc_e2e.c)
endif()

# Channel-1 external Read-Pin seam: the gpio_in_out button->LED sample but the
# button is driven as a real external driver (js_pal_gpio_read_state), not via
# the P3 latch. The node library calls back into the exported getter below to
# read the scripted level.
add_wink_wasm_mcs51_test(
    wasm_mcs51_gpio_external_test
    gpio_in_out
    ${_SDK_ROOT}/test/mcs51/test_mcs51_gpio_external_e2e.c
    "-sEXPORTED_FUNCTIONS=_main,_mcs51_wasm_ext_pin_state")
