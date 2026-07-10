/**
 * @file wink_blink_helper.h (samples/common compatibility shim)
 * @brief Forwarding include — BAL migration (ADR-0023 Task 2.1/2.5).
 *
 * @deprecated Include <output/wink_led_blink_helper.h> from wink_bal directly.
 *             This shim will be removed in the next release.
 *
 * The LED blink helper has moved to the BAL (Business Abstraction Layer).
 * New code should include <wink_blink_helper.h> directly from the wink_bal
 * public include surface; this shim exists so existing samples that still
 * #include "wink_blink_helper.h" via samples/common/include continue to
 * compile without source changes during Stage 2 migration.
 *
 * IMPORTANT: this shim MUST use its own distinct include guard. If it shared
 * WINK_BLINK_HELPER_H with the canonical BAL header and was found first in
 * the -I search order (as on ESP32 main, where WINK_APP_COMMON_INCLUDE_DIR
 * is added before the component's PUBLIC include dirs), the #define would
 * fire before the #include of the real header, and the real header's entire
 * body would be skipped by its own #ifndef guard.
 *
 * Copyright (c) 2026 Wink-AI.
 */
#ifndef WINK_BLINK_HELPER_SHIM_H
#define WINK_BLINK_HELPER_SHIM_H

/* Pull the canonical BAL header. It defines WINK_BLINK_HELPER_H on its own;
 * if the real header was already reached via a shorter -I path (e.g.
 * bal/include/output earlier in the search list), that's fine too — the
 * canonical header's own guard handles deduplication. */
#include "output/wink_blink_helper.h"

#endif /* WINK_BLINK_HELPER_SHIM_H */
