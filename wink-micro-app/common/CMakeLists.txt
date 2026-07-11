# samples/common — ADR-0023 Stage 2 migration complete.
#
# This directory no longer compiles any source files of its own. All helpers
# that used to live here have moved to their proper layers:
#   - wink_blink_helper / wink_button_helper / wink_telemetry_helper → BAL (bal/src/)
#   - wink_sim_ultrasonic_echo (S10 bringup helper) → runtime/selftest/src/
#
# The directory now exists purely as an INTERFACE library that:
#   1. Exposes samples/common/include/ so legacy #include "wink_*_helper.h"
#      paths continue to resolve through the forwarding shims.
#   2. PUBLIC-links wink_bal so blink/button/telemetry symbols resolve.
#   3. Adds runtime/selftest/src as an INTERFACE include dir so the
#      wink_sim_ultrasonic_echo.h shim (which uses a relative path to the
#      real header) resolves on any consumer.
#
# New code should NOT add new helpers here. Use BAL (Business Abstraction
# Layer) for product-grade helpers, or runtime/selftest/src for bringup/
# selftest-only helpers.

include_guard(GLOBAL)

set(WINK_SAMPLE_COMMON_INCLUDE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    CACHE INTERNAL "wink-micro-os samples/common include dir (forwarding shims)")

# INTERFACE library: no sources, only usage requirements (include dirs + link).
add_library(wink_sample_common INTERFACE)

# Expose the forwarding shim headers.
target_include_directories(wink_sample_common INTERFACE
    ${WINK_SAMPLE_COMMON_INCLUDE_DIR}
    # runtime/selftest/src is a PRIVATE include dir in selftest consumers'
    # own build rules, but the sim_ultrasonic_echo shim needs to resolve its
    # relative-path include to the real header. Add it as INTERFACE so any
    # consumer linking through wink_sample_common sees it.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/selftest/src
)

# Forward to BAL (blink/button/telemetry helpers) — PUBLIC because the shim
# headers include BAL headers and consumer code calls BAL symbols.
if(TARGET wink_bal)
    target_link_libraries(wink_sample_common INTERFACE wink_bal)
endif()
