/**
 * @file wink_button_helper.h (samples/common compatibility shim)
 * @brief Forwarding include — BAL migration (ADR-0023 Task 2.2).
 *
 * The button auto-poll helper has moved to the BAL (Business Abstraction
 * Layer). New code should include <wink_button_helper.h> directly from
 * the wink_bal public include surface; this shim exists so existing
 * samples that still #include "wink_button_helper.h" via
 * samples/common/include continue to compile without source changes
 * during Stage 2 migration.
 *
 * Copyright (c) 2026 Wink-AI.
 */
#ifndef WINK_BUTTON_HELPER_H
#define WINK_BUTTON_HELPER_H

/* Pull the canonical BAL header. It uses its own guard, but both files
 * intentionally share the same WINK_BUTTON_HELPER_H macro so that the
 * preprocessor short-circuits if the BAL header was already pulled in
 * via another include path. */
#include "input/wink_button_helper.h"

#endif /* WINK_BUTTON_HELPER_H */
