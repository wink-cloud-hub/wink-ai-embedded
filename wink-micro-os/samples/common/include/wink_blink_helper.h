/**
 * @file wink_blink_helper.h (samples/common compatibility shim)
 * @brief Forwarding include — BAL migration (ADR-0023 Task 2.1/2.5).
 *
 * The LED blink helper has moved to the BAL (Business Abstraction Layer).
 * New code should include <wink_blink_helper.h> directly from the wink_bal
 * public include surface; this shim exists so existing samples that still
 * #include "wink_blink_helper.h" via samples/common/include continue to
 * compile without source changes during Stage 2 migration.
 *
 * Copyright (c) 2026 Wink-AI.
 */
#ifndef WINK_BLINK_HELPER_H
#define WINK_BLINK_HELPER_H

/* Pull the canonical BAL header.  It uses its own guard, but both files
 * intentionally share the same WINK_BLINK_HELPER_H macro so that the
 * preprocessor short-circuits if the BAL header was already pulled in
 * via another include path. */
#include "output/wink_blink_helper.h"

#endif /* WINK_BLINK_HELPER_H */
