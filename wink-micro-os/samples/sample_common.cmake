# samples/sample_common.cmake — shared helpers for sample CMakeLists.txt
#
# P1-B3 (2026-07-04): extracted the ~40 lines of include-dir + compile-option
# boilerplate that every sample duplicated. Each sample CMakeLists.txt now
# calls wink_sample_apply_common(<target>) instead of copy-pasting the same
# 11 include directories and the MSVC / GCC compile-option branch.
#
# Design intent:
#   1. Include dirs are treated as a stable "sample include surface". Adding a
#      new DAL subdirectory (e.g. dal/include/comm) is a one-line change here
#      instead of touching 6 CMakeLists.
#   2. Compile options match the historical per-sample setting exactly (same
#      MSVC /W4 /WX + /wd4100 /wd4210 and GCC -Wall -Wextra -Werror
#      -Wno-unused-parameter). Behavior parity: no sample gets stricter or
#      laxer than before.
#   3. WINK_CONFIG_DIR (generated wink_config.h) is added when defined — the
#      resource_conflict sample does NOT consume it (no wink_app), so gating
#      on definedness keeps its include list clean.
#
# This file is include()d, not add_subdirectory()d, so functions/macros defined
# here are visible in the caller's scope.

include_guard(GLOBAL)

# ── Common source lists ───────────────────────────────────────────────────
# Runtime + trace core every "full" wink-app e2e binary drags in.
#
# Uses CACHE INTERNAL so the value survives across subdirectories: samples/
# is include()d from six add_subdirectory() calls, each of which is its own
# scope. Plain set() would only reach the first caller (because
# include_guard(GLOBAL) suppresses re-execution on subsequent include()s),
# leaving the variable empty in samples 2-6 and breaking their link step.
# CACHE INTERNAL is invisible to `cmake -L` (private impl detail) and never
# needs an explicit unset() from callers.
#
# Use with: ${WINK_SAMPLE_RUNTIME_SOURCES}
set(WINK_SAMPLE_RUNTIME_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/../runtime/src/wink_runtime.c
    ${CMAKE_CURRENT_LIST_DIR}/../runtime/src/wink_runtime_tasks.c
    ${CMAKE_CURRENT_LIST_DIR}/../runtime/src/wink_actuator_registry.c
    ${CMAKE_CURRENT_LIST_DIR}/../runtime/src/wink_soft_timer.c
    ${CMAKE_CURRENT_LIST_DIR}/../trace/src/wink_trace.c
    CACHE INTERNAL "wink-micro-os sample: runtime + trace core sources")

# ── Common include directories ────────────────────────────────────────────
# The full sample include surface: PAL headers (root + osal/hal), targets/common
# (wink_sim_physical.h etc.), all DAL subdirectories, runtime, trace, and the
# test tree (stubs used by host e2e drivers). Sample CMakeLists MAY prepend or
# append extra dirs (e.g. dual_task_demo adds targets/wasm) via the second arg.
function(wink_sample_apply_include_dirs target)
    # Extra include dirs beyond the shared surface (ARGN)
    set(_extra_dirs ${ARGN})

    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        # PAL: root + osal/ + hal/ subdirs (post-Phase-1 dir reorg)
        ${CMAKE_CURRENT_SOURCE_DIR}/../../pal/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../pal/include/osal
        ${CMAKE_CURRENT_SOURCE_DIR}/../../pal/include/hal
        # targets/common (wink_sim_physical.h and other cross-target headers)
        ${CMAKE_CURRENT_SOURCE_DIR}/../../targets/common/include
        # DAL: root + all category subdirs
        ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/include/input
        ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/include/output
        ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/include/actuator
        ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/include/display
        ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/include/sensor
        ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/include/communication
        ${CMAKE_CURRENT_SOURCE_DIR}/../../dal/include/storage
        # Runtime + trace + test (host e2e drivers live under test/)
        ${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../trace/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../test
        ${CMAKE_CURRENT_SOURCE_DIR}/../../test/stubs
        ${_extra_dirs}
    )

    # Generated wink_config.h: only present when a wink-app is in play. Sample
    # binaries that don't consume it (resource_conflict) skip this branch.
    if(DEFINED WINK_CONFIG_DIR)
        target_include_directories(${target} PRIVATE ${WINK_CONFIG_DIR})
    endif()
endfunction()

# ── Common compile options ────────────────────────────────────────────────
# Preserved exactly as each sample had them:
#   MSVC:   /W4 /WX + /wd4100 (unused params) + /wd4210 (file-scope static fn)
#   GCC:    -Wall -Wextra -Werror -Wno-unused-parameter
function(wink_sample_apply_compile_options target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /wd4100 /wd4210)
    else()
        # Missing-field-initializers fires on partial designated initializers
        # (.init/.loop/.on_fault without the newer .on_boot/.init_status/.on_fault_status).
        # C standard zero-initializes unspecified fields -- this is intentional.
        target_compile_options(${target} PRIVATE -Wall -Wextra -Werror
            -Wno-unused-parameter -Wno-missing-field-initializers)
    endif()
endfunction()

# ── Convenience: apply both to a sample target ────────────────────────────
# Most samples just want the full common surface. Extra include dirs may be
# passed after the target name (e.g. targets/wasm for dual_task_demo).
function(wink_sample_apply_common target)
    wink_sample_apply_include_dirs(${target} ${ARGN})
    wink_sample_apply_compile_options(${target})
endfunction()
