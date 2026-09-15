# SPDX-License-Identifier: LGPL-3.0-only
# mcs51_sources.cmake — single source of truth for MCS-51 framework target
# composition (Stage6 S6-1 Step 2/4, PLAN-20260911-MCS51-S6, CPL-15).
#
# Consumed by:
#   * frameworks/mcs51/CMakeLists.txt          (host/wasm library targets)
#   * frameworks/mcs51/test/wasm/add_wink_wasm_mcs51_test.cmake
#     (emcc+Node harness; stage6 deleted its hand-written copy)
#
# Paths are absolute (anchored on this file) so both include sites agree.

set(MCS51_FW_DIR "${CMAKE_CURRENT_LIST_DIR}")

# ── Generic core: Intel 8051/8052 semantics only (no vendor symbols) ─────────
set(MCS51_CORE_SOURCES
    "${MCS51_FW_DIR}/src/mcs51_context.cpp"
    "${MCS51_FW_DIR}/src/mcs51_family.cpp"
    "${MCS51_FW_DIR}/src/mcs51_peripheral.cpp"
    "${MCS51_FW_DIR}/src/mcs51_sfr.cpp"
    "${MCS51_FW_DIR}/src/mcs51_uni_bridge.cpp"
    "${MCS51_FW_DIR}/src/mcs51_adc.cpp"
    "${MCS51_FW_DIR}/src/mcs51_isr.cpp"
    "${MCS51_FW_DIR}/src/mcs51_clock.cpp"
    "${MCS51_FW_DIR}/src/mcs51_timer.cpp"
    "${MCS51_FW_DIR}/src/mcs51_uart.cpp"
    "${MCS51_FW_DIR}/src/mcs51_extint.cpp"
    "${MCS51_FW_DIR}/src/mcs51_xdata.cpp"
    "${MCS51_FW_DIR}/src/mcs51_unsupported.cpp"
    "${MCS51_FW_DIR}/src/mcs51_gpio.cpp"
    "${MCS51_FW_DIR}/src/mcs51_pcon.cpp"
    "${MCS51_FW_DIR}/src/mcs51_edge_queue.cpp"
    "${MCS51_FW_DIR}/src/mcs51_pwm_meter.cpp"
    "${MCS51_FW_DIR}/src/mcs51_bridge.cpp"
)

# ── CMS8S78xx chip package (vendor SFRs + on-chip peripherals) ───────────────
set(MCS51_CMS8S_SOURCES
    "${MCS51_FW_DIR}/chips/cms8s78xx/src/cms8s_register.cpp"
    "${MCS51_FW_DIR}/chips/cms8s78xx/src/cms8s_gpio.cpp"
    "${MCS51_FW_DIR}/chips/cms8s78xx/src/cms8s_extint.cpp"
    "${MCS51_FW_DIR}/chips/cms8s78xx/src/cms8s_uart.cpp"
    "${MCS51_FW_DIR}/chips/cms8s78xx/src/cms8s_timer.cpp"
    "${MCS51_FW_DIR}/chips/cms8s78xx/src/cms8s_adc.cpp"
    "${MCS51_FW_DIR}/chips/cms8s78xx/src/cms8s_acmp.cpp"
    "${MCS51_FW_DIR}/chips/cms8s78xx/src/cms8s_buzzer.cpp"
    "${MCS51_FW_DIR}/chips/cms8s78xx/src/cms8s_sys.cpp"
)

# ── Classic AT89C52 package: empty register entry keeps the protocol uniform ─
set(MCS51_AT89_SOURCES
    "${MCS51_FW_DIR}/chips/at89c52/src/at89_register.cpp"
)

# ── Board device: ADC0832 external 8-bit ADC (trap-attached) ─────────────────
set(MCS51_ADC0832_SOURCES
    "${MCS51_FW_DIR}/devices/adc0832/src/mcs51_adc0832.cpp"
)

# Wasm test harness variant: the UniSim host-side bridge glue is not part of
# the emcc+Node sandbox (mcs51_wasm_link_stubs.c plays that role there).
set(MCS51_CORE_WASM_SOURCES ${MCS51_CORE_SOURCES})
list(REMOVE_ITEM MCS51_CORE_WASM_SOURCES
    "${MCS51_FW_DIR}/src/mcs51_uni_bridge.cpp")

# Framework-owned include surface for the wasm harness (the harness appends
# PAL/DAL/runtime/target dirs on top).
set(MCS51_WASM_INCLUDE_DIRS
    "${MCS51_FW_DIR}/include"
    # test-only family harness (shared host/wasm e2e drivers include it).
    "${MCS51_FW_DIR}/test"
    # chip-private + board-device public headers (same set the library targets
    # compile with; the wasm harness links one flat binary like the old list).
    "${MCS51_FW_DIR}/chips/cms8s78xx/include"
    "${MCS51_FW_DIR}/chips/at89c52/include"
    "${MCS51_FW_DIR}/devices/adc0832/include"
)
