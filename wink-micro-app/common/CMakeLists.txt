# wink-micro-app/common — ADR-0038 / ADR-0023 sample INTERFACE only.
#
# No local sources. Product BAL APIs live in wink-micro-os/bal/.
# Bringup-only ultrasonic echo sim lives in runtime/selftest/src/.
#
# This INTERFACE library:
#   1. Adds runtime/selftest/src so apps can #include "wink_sim_ultrasonic_echo.h"
#      (bringup/selftest internal — not a BAL public API).
#   2. PUBLIC-links wink_bal so domain-prefixed BAL headers resolve
#      (e.g. "output/wink_led_blink.h", "sensor/wink_ultrasonic_poll.h").
#
# New code: use BAL for product services; use runtime/selftest only for
# bringup/selftest. Do not add forwarding shims here.

include_guard(GLOBAL)

set(WINK_SAMPLE_COMMON_INCLUDE_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    CACHE INTERNAL "wink-micro-app/common include dir")

add_library(wink_sample_common INTERFACE)

target_include_directories(wink_sample_common INTERFACE
    ${WINK_SAMPLE_COMMON_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../wink-micro-os/runtime/selftest/src
)

if(TARGET wink_bal)
    target_link_libraries(wink_sample_common INTERFACE wink_bal)
endif()
