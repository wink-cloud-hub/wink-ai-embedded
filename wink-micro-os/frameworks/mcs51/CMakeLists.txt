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
#
# Stage6 split (PLAN-20260911-MCS51-S6, CPL-15/24): the former monolithic
# wink_mcs51_compat is now four single-responsibility STATIC targets
#   wink_mcs51_core / _cms8s / _at89 / _adc0832
# plus per-family STRICT twins (WINK_MCS51_STRICT is a per-target compile
# definition, never a framework-wide cache knob — no 2xN library matrix).
# The legacy monolithic names survive one version as INTERFACE aliases for
# rollback (R-04) and stage7 removal. Source lists live in mcs51_sources.cmake
# so the emcc+Node test harness compiles the exact same set.
# ─────────────────────────────────────────────────────────────────────────────

if(ESP_PLATFORM)
    # Real-hardware build: mcs51 interception is host/wasm-only. Do not register
    # any component or source — guarantees zero footprint in the ESP32 image.
    message(STATUS "[frameworks] mcs51 interception skipped on ESP_PLATFORM (sim-only)")
    return()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(${CMAKE_CURRENT_LIST_DIR}/mcs51_sources.cmake)

# The chip register entry is built as an OBJECT library, linked directly by
# the final executable (tests / production app). Reason: mcs51_bridge.cpp
# (core) calls the chosen family register entry (generated
# mcs51_family_select.h), while the register entry and chip models call back
# into core — an object-level cycle that a single-pass static-archive scan
# cannot resolve. Objects are placed on the link line ahead of the archives,
# so the forward reference resolves without whole-archive or link-group flags
# (portable down to CMake 3.15 / MSVC). Object libraries are not transitive
# through INTERFACE_LINK_LIBRARIES, so consumers list it explicitly.
set(_MCS51_CMS8S_LIB_SOURCES ${MCS51_CMS8S_SOURCES})
list(REMOVE_ITEM _MCS51_CMS8S_LIB_SOURCES
    "${MCS51_FW_DIR}/chips/cms8s78xx/src/cms8s_register.cpp")

# ── Four single-responsibility targets (STATIC EXCLUDE_FROM_ALL) ─────────────
# EXCLUDE_FROM_ALL: only built when an app/test links them (mirrors
# wink_arduino_compat). at89 stays a STATIC library even though its single TU
# is an empty register entry: INTERFACE targets cannot compile .cpp, and the
# four targets must stay attribute-symmetric (total §3.2).
add_library(wink_mcs51_core    STATIC EXCLUDE_FROM_ALL ${MCS51_CORE_SOURCES})
add_library(wink_mcs51_cms8s   STATIC EXCLUDE_FROM_ALL ${_MCS51_CMS8S_LIB_SOURCES})
add_library(wink_mcs51_at89    STATIC EXCLUDE_FROM_ALL ${MCS51_AT89_SOURCES})
add_library(wink_mcs51_adc0832 STATIC EXCLUDE_FROM_ALL ${MCS51_ADC0832_SOURCES})
add_library(wink_mcs51_cms8s_register OBJECT EXCLUDE_FROM_ALL
    "${MCS51_FW_DIR}/chips/cms8s78xx/src/cms8s_register.cpp")

# STRICT twins: only families carrying STRICT-gated code get one (at89 and
# adc0832 have none). The macro is PUBLIC so STRICT test TUs see it too; the
# release twins are unaffected. Never ship STRICT (debug/test configuration).
add_library(wink_mcs51_core_strict  STATIC EXCLUDE_FROM_ALL ${MCS51_CORE_SOURCES})
add_library(wink_mcs51_cms8s_strict STATIC EXCLUDE_FROM_ALL ${_MCS51_CMS8S_LIB_SOURCES})
target_compile_definitions(wink_mcs51_core_strict  PUBLIC WINK_MCS51_STRICT=1)
target_compile_definitions(wink_mcs51_cms8s_strict PUBLIC WINK_MCS51_STRICT=1)

# Legacy monolithic names, alias-only for one version (R-04 rollback path).
# The aliases bundle the STATIC libraries only: CMake does not propagate
# OBJECT-library objects through INTERFACE_LINK_LIBRARIES, so the chip
# register object (wink_mcs51_<family>_register) must be linked by the final
# executable that needs chip registration (tests do this explicitly). Both
# aliases are symmetric (core/family/at89/adc0832); production injection uses
# the per-family wink_mcs51_inject_<family> bundles below.
add_library(wink_mcs51_compat INTERFACE)
target_link_libraries(wink_mcs51_compat INTERFACE
    wink_mcs51_core wink_mcs51_cms8s wink_mcs51_at89 wink_mcs51_adc0832)
add_library(wink_mcs51_compat_strict INTERFACE)
target_link_libraries(wink_mcs51_compat_strict INTERFACE
    wink_mcs51_core_strict wink_mcs51_cms8s_strict wink_mcs51_at89
    wink_mcs51_adc0832)

# Stage6 review S6-H1: per-family production injection bundle. The root build
# resolves wink-app.json mcu -> manifest family and links
# wink_mcs51_inject_<family> (+ the conventional register object below), so a
# new chip needs no root CMake edit.
add_library(wink_mcs51_inject_cms8s78xx INTERFACE)
target_link_libraries(wink_mcs51_inject_cms8s78xx INTERFACE wink_mcs51_cms8s)
add_library(wink_mcs51_inject_at89c52 INTERFACE)
target_link_libraries(wink_mcs51_inject_at89c52 INTERFACE wink_mcs51_at89)

# Conventional register-object name per manifest family (root links
# "wink_mcs51_<family>_register" when it exists; discovered families already
# use that name, cms8s needs this alias for its canonical target name).
add_library(wink_mcs51_cms8s78xx_register ALIAS wink_mcs51_cms8s_register)

# ── Per-compiler flag chain (applied to every build target) ──────────────────
# Proven on GCC 14.2 / MSVC 14.40 / emcc 4.0.5 in Spike-S2
# (docs/tech-designs/mcs51/spikes/S2-compiler-dialect-chain.md §3).
#   - Cleanup emits .cpp, so NO -x c++ (GCC) / LANGUAGE CXX (MSVC) is needed.
#   - NO -fpermissive anywhere (would let host false-pass).
#   - NO -Wno-pointer-sign (C-only flag, warns under C++).
function(wink_mcs51_apply_common tgt)
    if(MSVC)
        target_compile_options(${tgt} PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:/std:c++17 /utf-8 /W3 /wd4101>
        )
        # Erase GCC-style attributes the sandbox headers may reference.
        target_compile_definitions(${tgt} PRIVATE "__attribute__(x)=")
    else()
        target_compile_options(${tgt} PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:-std=c++17 -Wno-write-strings>
        )
        # Sandbox .cpp follows the no-exceptions/no-RTTI subset policy
        # (ADR-0036).
        target_compile_options(${tgt} PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti -fno-threadsafe-statics>
        )
    endif()
    # Cooperative yield uses the PAL sleep bridge (same primitive Arduino
    # uses); M2 adds the virtual-microsecond quota + timer catch-up on top.
    target_link_libraries(${tgt} PUBLIC pal)
endfunction()

foreach(_mcs51_tgt
        wink_mcs51_core wink_mcs51_cms8s wink_mcs51_at89 wink_mcs51_adc0832
        wink_mcs51_cms8s_register
        wink_mcs51_core_strict wink_mcs51_cms8s_strict)
    wink_mcs51_apply_common(${_mcs51_tgt})
endforeach()

# ── Include surfaces + intra-stack links ─────────────────────────────────────
# Core: public sandbox headers + runtime event/app API; targets/common stays
# private (fiber-context detection in the clock engine).
foreach(_mcs51_core_tgt wink_mcs51_core wink_mcs51_core_strict)
    target_include_directories(${_mcs51_core_tgt}
        PUBLIC include
        PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../../runtime/include
        PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../targets/common/include
    )
    # Core uses the event queue (bridge/pcon/isr: wink_event_*); declaring the
    # dependency keeps the link order right for static single-pass scans.
    target_link_libraries(${_mcs51_core_tgt} PUBLIC wink_runtime)
endforeach()

# Chip packages: own private headers only; public sandbox headers come from
# core. STRICT twins link their STRICT core twin so a strict test binary never
# carries two core copies (duplicate symbols).
foreach(_mcs51_cms8s_tgt wink_mcs51_cms8s wink_mcs51_cms8s_strict)
    target_include_directories(${_mcs51_cms8s_tgt}
        PRIVATE ${CMAKE_CURRENT_LIST_DIR}/chips/cms8s78xx/include
    )
endforeach()
target_link_libraries(wink_mcs51_cms8s        PUBLIC wink_mcs51_core)
target_link_libraries(wink_mcs51_cms8s_strict PUBLIC wink_mcs51_core_strict)

target_include_directories(wink_mcs51_cms8s_register
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}/chips/cms8s78xx/include)
target_include_directories(wink_mcs51_cms8s_register
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)
target_include_directories(wink_mcs51_cms8s_register
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../runtime/include)
target_link_libraries(wink_mcs51_cms8s_register PRIVATE pal)

target_include_directories(wink_mcs51_at89
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}/chips/at89c52/include
)
# at89 deliberately links no core: classic is the zero-extension pure core and
# this package's only TU is the empty register entry. Linking core here would
# drag the release core into STRICT test binaries (duplicate symbols); the
# consumer (app/test alias) always provides core explicitly.

# ── New-chip drill support (S6-1 Step 5): package auto-discovery ─────────────
# A new 51 family lands as `chips/<family>/` (+ its tools/manifests/chips
# entry, S6-2) with NO edit here: every chip directory not explicitly named
# above becomes `wink_mcs51_<family>` (STATIC, core-linked, same dialect).
# Review S6-H2: a `*_register.cpp` in the package is split into the
# conventional `wink_mcs51_<family>_register` OBJECT target, so a new family
# with the standard register protocol never re-introduces the core<->chip
# object-level link cycle. Families needing a canonical target name
# (cms8s78xx -> wink_mcs51_cms8s) or STRICT-gated hooks stay explicit above.
file(GLOB _mcs51_chip_dirs LIST_DIRECTORIES true
    "${CMAKE_CURRENT_LIST_DIR}/chips/*")
foreach(_mcs51_chip_dir IN LISTS _mcs51_chip_dirs)
    if(NOT IS_DIRECTORY "${_mcs51_chip_dir}")
        continue()
    endif()
    get_filename_component(_mcs51_family "${_mcs51_chip_dir}" NAME)
    if(_mcs51_family STREQUAL "cms8s78xx" OR _mcs51_family STREQUAL "at89c52")
        continue()  # explicit targets above
    endif()
    file(GLOB_RECURSE _mcs51_chip_srcs CONFIGURE_DEPENDS
        "${_mcs51_chip_dir}/src/*.cpp" "${_mcs51_chip_dir}/src/*.c")
    if(NOT _mcs51_chip_srcs)
        continue()
    endif()
    set(_mcs51_chip_tgt "wink_mcs51_${_mcs51_family}")
    set(_mcs51_chip_reg_tgt "")
    file(GLOB _mcs51_chip_reg_srcs CONFIGURE_DEPENDS
        "${_mcs51_chip_dir}/src/*_register.cpp")
    if(_mcs51_chip_reg_srcs)
        list(GET _mcs51_chip_reg_srcs 0 _mcs51_chip_reg_src)
        list(REMOVE_ITEM _mcs51_chip_srcs "${_mcs51_chip_reg_src}")
        set(_mcs51_chip_reg_tgt "${_mcs51_chip_tgt}_register")
        add_library(${_mcs51_chip_reg_tgt} OBJECT EXCLUDE_FROM_ALL
            "${_mcs51_chip_reg_src}")
        wink_mcs51_apply_common(${_mcs51_chip_reg_tgt})
        target_include_directories(${_mcs51_chip_reg_tgt}
            PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include
            PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../runtime/include)
        if(IS_DIRECTORY "${_mcs51_chip_dir}/include")
            target_include_directories(${_mcs51_chip_reg_tgt}
                PRIVATE "${_mcs51_chip_dir}/include")
        endif()
    endif()
    if(_mcs51_chip_srcs)
        add_library(${_mcs51_chip_tgt} STATIC EXCLUDE_FROM_ALL ${_mcs51_chip_srcs})
        wink_mcs51_apply_common(${_mcs51_chip_tgt})
        target_include_directories(${_mcs51_chip_tgt}
            PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)
        if(IS_DIRECTORY "${_mcs51_chip_dir}/include")
            target_include_directories(${_mcs51_chip_tgt}
                PRIVATE "${_mcs51_chip_dir}/include")
        endif()
        target_link_libraries(${_mcs51_chip_tgt} PUBLIC wink_mcs51_core)
        add_library("wink_mcs51_inject_${_mcs51_family}" INTERFACE)
        target_link_libraries("wink_mcs51_inject_${_mcs51_family}" INTERFACE
            ${_mcs51_chip_tgt})
        message(STATUS
            "MCS-51 chip package auto-discovered: ${_mcs51_family} -> ${_mcs51_chip_tgt}")
    elseif(_mcs51_chip_reg_tgt)
        # Register-only shell (drill/new-family bootstrap): models land later;
        # the register object alone is a valid package stage.
        message(STATUS
            "MCS-51 chip package auto-discovered (register-only shell): "
            "${_mcs51_family} -> ${_mcs51_chip_reg_tgt}")
    else()
        message(STATUS
            "MCS-51 chip directory ignored (no sources): ${_mcs51_family}")
    endif()
endforeach()

# Board device: the ADC0832 attach API is public (boards/tests bind it).
# Review S6-H3: like at89, the device deliberately links no core — its core
# symbols resolve from the consumer (which always links core), so the device
# can ride the STRICT alias without dragging the release core into a strict
# test binary.
target_include_directories(wink_mcs51_adc0832
    PUBLIC ${CMAKE_CURRENT_LIST_DIR}/devices/adc0832/include
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../runtime/include
)

# ── Configurable knobs ───────────────────────────────────────────────────────
# S6-3 (CPL-24): the former framework-wide WINK_MCS51_XDATA_SIZE cache knob is
# gone — the aperture is board scope now. absacc.h keeps the 8192 default for
# builds without a board define; boards/apps set WINK_MCS51_XDATA_SIZE as a
# per-app compile definition (documented in design 02/03).

message(STATUS
    "Configured MCS-51 targets: core/cms8s/at89/adc0832 (+ strict core/cms8s)")
