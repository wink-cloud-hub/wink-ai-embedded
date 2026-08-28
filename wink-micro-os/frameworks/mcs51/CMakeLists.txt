cmake_minimum_required(VERSION 3.15)

# ─────────────────────────────────────────────────────────────────────────────
# MCS-51 / 8051 zero-code simulation interception layer (Axis B, ADR-0070).
#
# wink itself NEVER runs on real 8051 hardware. Unmodified Keil C51 user
# sources are regex-cleaned to .cpp copies in the build tree and compiled as
# C++17 into the host/wasm sandbox behind SFR proxies + cooperative fibers.
#
# HARD GATE: this layer is simulation-only. It must never compile into a real
# firmware target (ESP-IDF/xtensa). On ESP_PLATFORM we return before creating
# any target, so the ESP32 link map carries zero mcs51 symbols (M1 DoD).
# ─────────────────────────────────────────────────────────────────────────────

if(ESP_PLATFORM)
    # Real-hardware build: mcs51 interception is host/wasm-only. Do not register
    # any component or source — guarantees zero footprint in the ESP32 image.
    message(STATUS "[frameworks] mcs51 interception skipped on ESP_PLATFORM (sim-only)")
    return()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Host/wasm standalone target. EXCLUDE_FROM_ALL: only built when an app/test
# links it (mirrors wink_arduino_compat).
add_library(wink_mcs51_compat STATIC EXCLUDE_FROM_ALL
    src/mcs51_sfr.cpp
    src/mcs51_uni_bridge.cpp
    src/mcs51_adc.cpp
    src/mcs51_adc0832.cpp
    src/mcs51_isr.cpp
    src/mcs51_clock.cpp
    src/mcs51_timer.cpp
    src/mcs51_uart.cpp
    src/mcs51_xdata.cpp
    src/mcs51_unsupported.cpp
    src/mcs51_bridge.cpp
)

# ── M3 configurable knobs (cache options) ────────────────────────────────────
# Legal XDATA aperture for XBYTE/XWORD (bytes); spec band 4–16 KB, default 8 KB.
set(WINK_MCS51_XDATA_SIZE 8192 CACHE STRING
    "MCS-51 sim: legal XDATA aperture size in bytes (XBYTE/XWORD; 4096..16384)")
target_compile_definitions(wink_mcs51_compat PUBLIC
    WINK_MCS51_XDATA_SIZE=${WINK_MCS51_XDATA_SIZE})

# STRICT dual-mode unsupported-feature mechanism (plan prose: WINK_SIM_STRICT).
# OFF (default): unmodeled features log one pal_log_w warning and continue.
# ON: the same call sites assert — a debug/test configuration that fails loudly.
option(WINK_MCS51_STRICT
    "MCS-51 sim: assert on unsupported 8051 features instead of warning (debug/test only)"
    OFF)
if(WINK_MCS51_STRICT)
    target_compile_definitions(wink_mcs51_compat PUBLIC WINK_MCS51_STRICT=1)
endif()

target_include_directories(wink_mcs51_compat
    PUBLIC include
    # wink_app.h (callback binding). The runtime LIBRARY is deliberately NOT
    # linked here — the final executable (simulator or test) provides it — so we
    # only add the header path.
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../runtime/include
    # wink_sim_scheduler.h (fiber-context detection in the clock engine).
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../targets/common/include
)

# Per-compiler flag chain — proven on GCC 14.2 / MSVC 14.40 / emcc 4.0.5 in
# Spike-S2 (docs/tech-designs/mcs51/spikes/S2-compiler-dialect-chain.md §3).
#   - Cleanup emits .cpp, so NO -x c++ (GCC) / LANGUAGE CXX (MSVC) is needed.
#   - NO -fpermissive anywhere (would let host false-pass).
#   - NO -Wno-pointer-sign (C-only flag, warns under C++).
if(MSVC)
    target_compile_options(wink_mcs51_compat PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/std:c++17 /utf-8 /W3 /wd4101>
    )
    # Erase GCC-style attributes the sandbox headers may reference.
    target_compile_definitions(wink_mcs51_compat PRIVATE "__attribute__(x)=")
else()
    target_compile_options(wink_mcs51_compat PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-std=c++17 -Wno-write-strings>
    )
endif()

# Sandbox .cpp follows the no-exceptions/no-RTTI subset policy (ADR-0036).
if(NOT MSVC)
    target_compile_options(wink_mcs51_compat PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti -fno-threadsafe-statics>
    )
endif()

# Cooperative yield uses the PAL sleep bridge (same primitive Arduino uses);
# M2 adds the virtual-microsecond quota + timer catch-up on top.
target_link_libraries(wink_mcs51_compat PUBLIC pal)

message(STATUS "Configured wink_mcs51_compat target (MCS-51 sim interception, C++17)")
