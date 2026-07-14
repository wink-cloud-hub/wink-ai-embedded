/**
 * @file wink_button_helper.c
 * @brief BAL button auto-poll helper — thin backwards-compat wrapper
 *        around the newer `wink_button_enable_events` API (ADR-0032).
 *
 * All the slot pool, periodic tick, and event-dispatch logic now lives in
 * `wink_button_events.c`. This TU exists only to keep the two-argument
 * `wink_button_helper_start(btn, poll_ms)` signature working for callers
 * that predate `wink_button_enable_events` (e.g. `test_button_helper.c`,
 * `devkitc_smoke/app_callbacks.c`, and the codegen L2 verb
 * `start_auto_poll`). New callers should target `wink_button_enable_events`
 * directly.
 *
 * The default `debounce_ms = 20` here mirrors ADR-0031's documented
 * default for JSON `debounce_ms`, so codegen-emitted L1 wrappers and
 * this manual L2 API agree on "no explicit override" semantics.
 *
 * Note: helper.c calls the *primary* `wink_button_enable_events` /
 * `wink_button_disable_events` names — not the deprecated
 * `wink_button_events_start` / `_stop` inline shims — so BAL's own
 * build stays free of deprecation warnings.
 *
 * Copyright (c) 2026 Wink-AI.
 */
#include "wink_button_helper.h"
#include "wink_button_events.h"
#include "wink_status.h"

#include <stddef.h>
#include <stdbool.h>

/* The helper API itself is marked WINK_DEPRECATED in the header for
 * downstream callers; suppress the corresponding warning here in the
 * implementation TU so BAL's own build stays clean. */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

wink_status_t wink_button_helper_start(dal_button_t *btn, uint32_t poll_ms)
{
    /* Preserve the historical contract: poll_ms == 0 is rejected here
     * (enable_events would also catch it via the SOFT_POLL branch, but
     * this keeps the helper's diagnostic surface unchanged for callers
     * relying on the specific INVALID_ARG return). */
    if (btn == NULL || poll_ms == 0u) {
        return WINK_ERR_INVALID_ARG;
    }

    const wink_button_event_config_t cfg = {
        .drive           = WINK_BUTTON_DRIVE_SOFT_POLL,
        .auto_poll_ms    = poll_ms,
        .debounce_ms     = 20u,   /* ADR-0031 default; L2 API cannot override */
        .wake_from_sleep = false,
    };
    return wink_button_enable_events(btn, &cfg);
}

wink_status_t wink_button_helper_stop(dal_button_t *btn)
{
    /* disable_events is NULL-safe and idempotent. Historical helper_stop
     * signature returns wink_status_t so we keep returning WINK_OK. */
    wink_button_disable_events(btn);
    return WINK_OK;
}

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
