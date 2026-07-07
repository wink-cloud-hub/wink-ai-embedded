/**
 * @file wink_sim_ultrasonic_echo.h (samples/common compatibility shim)
 * @brief Forwarding include — runtime/selftest migration (ADR-0023 Task 2.4/2.5).
 *
 * The S10 ultrasonic echo-simulator helper has moved to runtime/selftest/src/
 * (a bringup/selftest internal helper, NOT a stable public API). This shim
 * exists so existing samples that #include "wink_sim_ultrasonic_echo.h"
 * via samples/common/include continue to compile without source changes
 * during Stage 2 migration.
 *
 * STRICT_NONBLOCKING: the target header wraps its declarations with
 * #ifndef WINK_STRICT_NONBLOCKING, so strict-mirror TUs see nothing.
 *
 * IMPORTANT: this shim uses its own distinct include guard (same rationale
 * as wink_blink_helper.h shim) — sharing WINK_SIM_ULTRASONIC_ECHO_H with
 * the canonical header causes the real header body to be skipped when
 * common/include precedes runtime/selftest/src in the -I search order.
 *
 * Copyright (c) 2026 Wink-AI.
 */
#ifndef WINK_SIM_ULTRASONIC_ECHO_SHIM_H
#define WINK_SIM_ULTRASONIC_ECHO_SHIM_H

/* Pull the canonical header from its new home under runtime/selftest/src/.
 * A relative path is used because runtime/selftest/src is a PRIVATE include
 * dir in selftest consumers' build rules, not a top-level public surface;
 * the shim's INTERFACE include on wink_sample_common adds that directory
 * to any consumer's search path, but the relative form keeps this working
 * even when invoked through a different include path. */
#include "../../../runtime/selftest/src/wink_sim_ultrasonic_echo.h"

#endif /* WINK_SIM_ULTRASONIC_ECHO_SHIM_H */
