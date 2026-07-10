/**
 * @file wink_button_helper.h (samples/common compatibility shim)
 * @brief Forwarding include — BAL migration (ADR-0023 Task 2.2/2.5).
 *
 * @deprecated Include <input/wink_button_helper.h> from wink_bal directly.
 *             This shim will be removed in the next release.
 *
 * The button auto-poll helper has moved to the BAL (Business Abstraction
 * Layer). New code should include <wink_button_helper.h> directly from
 * the wink_bal public include surface; this shim exists so existing
 * samples that still #include "wink_button_helper.h" via
 * samples/common/include continue to compile without source changes
 * during Stage 2 migration.
 *
 * IMPORTANT: see note on wink_blink_helper.h — this shim MUST use its own
 * distinct include guard to avoid preprocessor-guard collision with the
 * canonical BAL header when common/include precedes bal/include/input in
 * the -I search order (as on ESP32 main).
 *
 * Copyright (c) 2026 Wink-AI.
 */
#ifndef WINK_BUTTON_HELPER_SHIM_H
#define WINK_BUTTON_HELPER_SHIM_H

#include "input/wink_button_helper.h"

#endif /* WINK_BUTTON_HELPER_SHIM_H */
