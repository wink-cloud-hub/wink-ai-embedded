/**
 * @file wink_default_telemetry.h (samples/common compatibility shim)
 * @brief Forwarding include — BAL migration (ADR-0023 Task 2.3/2.5).
 *
 * The default telemetry helper has moved to the BAL (Business Abstraction
 * Layer).  New code should include <comm/wink_telemetry_helper.h> directly
 * and use wink_telemetry_default_start(); this shim exists so existing
 * samples that still #include "wink_default_telemetry.h" continue to
 * compile during Stage 2-3 migration.
 *
 * Copyright (c) 2026 Wink-AI.
 */
#ifndef WINK_DEFAULT_TELEMETRY_H
#define WINK_DEFAULT_TELEMETRY_H

#include "comm/wink_telemetry_helper.h"

/* Compatibility alias: the old name maps to the new BAL entry point.
 * The stop/is_running APIs did not exist in the old helper (it was a
 * fire-and-forget singleton), so only _start needs an alias. */
#define wink_default_telemetry_start(sonar, btn) \
    wink_telemetry_default_start((sonar), (btn))

#endif /* WINK_DEFAULT_TELEMETRY_H */
