# PAL (Platform Abstraction Layer) — Interface contracts + target-independent algorithm TUs.
#
# Three-tier structure:
#   1. `pal`         INTERFACE — Pure interface contract (headers + include dirs).
#   2. `PAL_COMMON_SOURCES` CACHE variable — SSOT for target-independent PAL translation units.
#   3. `pal_common`  OBJECT — OBJECT library for top-level executables (wink_simulator).

add_library(pal INTERFACE)

# Expose PAL contract headers to downstream targets
target_include_directories(pal INTERFACE
    include
    include/osal
    include/hal
)

# Generated configuration header (wink_config.h) directory
target_include_directories(pal INTERFACE ${WINK_CONFIG_DIR})

# ── target-independent PAL sources SSOT ────────────────────────────────
set(PAL_COMMON_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/pal_pwm_router.c
    CACHE INTERNAL "wink-micro-os: target-independent PAL sources SSOT")

# ── pal_common OBJECT ─────────────────────────────────────────────────
add_library(pal_common OBJECT ${PAL_COMMON_SOURCES})
target_link_libraries(pal_common PUBLIC pal)
