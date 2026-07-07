/**
 * @file wink_bal_stub.c
 * @brief Stage 1 placeholder — empty translation unit so wink_bal links.
 *
 * Replaced in Stage 2-3 when the real helpers (led_blink, button, sonar,
 * servo, telemetry, oled) migrate in from samples/common/.
 *
 * Copyright (c) 2026 Wink-AI.
 */

/* Portable "used" attribute for the placeholder symbol, so ar/ranlib
 * don't warn about an empty archive on picky toolchains. */
#if defined(__GNUC__) || defined(__clang__)
#  define WINK_BAL_USED __attribute__((used))
#elif defined(_MSC_VER)
#  define WINK_BAL_USED
#else
#  define WINK_BAL_USED
#endif

/* Non-static, file-scoped symbol — guarantees the TU is non-empty and
 * gets pulled into the archive without producing any link pollution
 * (nothing references it, so it's dead-stripped at final link). */
WINK_BAL_USED int wink_bal_stub_placeholder = 0;
