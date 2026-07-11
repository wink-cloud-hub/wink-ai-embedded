# unisim_smoke: fixture App for Phase B Node smoke test.
# Reaches all 13 js_* imports from a single app_init() so the JS-side test
# only needs to instantiate + advance the clock once.
#
# Shared boilerplate in ../sample_common.cmake (host-target helpers aren't
# used here — this sample only exists for wasm smoke — but including keeps
# the file discoverable via the same convention as its siblings).
include(${CMAKE_CURRENT_LIST_DIR}/../sample_common.cmake)

set(APP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/device_tree.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app_callbacks.c)

# Export to top-level wink_simulator (wasm) — same contract as avoidance_car.
set(WINK_APP_SOURCES ${APP_SOURCES} PARENT_SCOPE)

# host branch: no e2e binary — this sample only exists for wasm smoke.
# (devkitc_smoke does the reverse: FATAL_ERROR on EMSCRIPTEN. Here we
#  simply return so host build tree contains no target for this sample.)
if(NOT EMSCRIPTEN)
    return()
endif()
