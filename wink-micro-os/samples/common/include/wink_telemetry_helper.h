/**
 * @file wink_telemetry_helper.h (samples/common compatibility shim)
 * @brief Forwarding include — BAL migration (ADR-0023 Task 2.3/2.5).
 *
 * The telemetry helper has moved to the BAL (Business Abstraction Layer).
 * New code should include <comm/wink_telemetry_helper.h> directly from the
 * wink_bal public include surface; this shim exists so samples that
 * #include "wink_telemetry_helper.h" via samples/common/include continue
 * to compile without source changes during Stage 2 migration.
 *
 * IMPORTANT: this shim uses its own distinct include guard (same rationale
 * as wink_blink_helper.h shim) — sharing WINK_TELEMETRY_HELPER_H with the
 * canonical BAL header causes the real header body to be skipped when
 * common/include precedes bal/include/comm in the -I search order.
 *
 * Copyright (c) 2026 Wink-AI.
 */
#ifndef WINK_TELEMETRY_HELPER_SHIM_H
#define WINK_TELEMETRY_HELPER_SHIM_H

#include "comm/wink_telemetry_helper.h"

#endif /* WINK_TELEMETRY_HELPER_SHIM_H */
