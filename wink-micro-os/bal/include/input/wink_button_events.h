/**
 * @file wink_button_events.h
 * @brief BAL public API — button event stream (soft-poll today, GPIO-IRQ
 *        gated behind ADR-0031's `event_drive` selector).
 *
 * This is the single entry point that codegen emits for the L1 verb
 * `enable_events`. It replaces the older direct `wink_button_helper_*`
 * call: `wink_button_helper_start(&btn, poll_ms)` is now a thin backwards-
 * compat wrapper around `wink_button_events_start(&btn, &cfg)` with a
 * fixed soft-poll config.
 *
 * The public API is intentionally cfg-shaped rather than parameterised
 * so that:
 *   1. Future knobs (deep-sleep wake, IRQ prio, coalescing window) can be
 *      added without breaking the ABI — codegen just emits a new field
 *      and old .h stays valid.
 *   2. AI / lowcode generators can produce a single, static, greppable
 *      `wink_button_event_config_t` per button.
 *
 * Layering (ADR-0023 §8): this header MUST NOT include any pal_*.h.
 * GPIO IRQ registration is done inside the .c and stays behind this API.
 *
 * @warning **Callback context (read before using):**
 *    Event callbacks registered via dal_button_on_event() fire from
 *    inside the periodic LIGHT (soft-timer) dispatch or a GPIO ISR,
 *    both of which have ISR-like constraints:
 *      - No blocking or yielding APIs.
 *      - < ~100 us of work.
 *    Post to the event queue (WINK_EVENT_BUTTON_*) and process from
 *    app_loop / on_event().
 *
 * Copyright (c) 2026 Wink-AI.
 */
#ifndef WINK_BUTTON_EVENTS_H
#define WINK_BUTTON_EVENTS_H

#include <stdbool.h>
#include <stdint.h>
#include "wink_status.h"
#include "dal_button.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Slot-pool size for concurrent button event streams.
 *
 * Codegen can size this precisely from the device tree via
 * `-DWINK_BUTTON_EVENTS_MAX=N`. Defaults to 4 to match the historical
 * WINK_BUTTON_HELPER_MAX so the thin helper wrapper never runs out
 * before the events API does.
 */
#ifndef WINK_BUTTON_EVENTS_MAX
#define WINK_BUTTON_EVENTS_MAX 4
#endif

/**
 * @brief Event source selector — how PRESS/RELEASE/LONG_PRESS are
 *        detected by the runtime for a given button.
 *
 * See ADR-0031 for the option comparison and rationale.
 */
typedef enum {
    WINK_BUTTON_DRIVE_SOFT_POLL = 0, /**< Periodic soft-timer poll (portable, default). */
    WINK_BUTTON_DRIVE_GPIO_IRQ  = 1, /**< GPIO edge IRQ + short debounce follow-up poll (ESP32-only in S3+). */
} wink_button_event_drive_t;

/**
 * @brief Config for one button's event stream.
 *
 * Populated by codegen from `wink-app.json` fields (ADR-0031), or by
 * the thin helper wrapper for legacy `wink_button_helper_start` callers.
 *
 * @note All members are POD; the struct is designed to be `static const`
 *       at TU scope so codegen output is fully constant-foldable.
 */
typedef struct {
    /** Event source. `SOFT_POLL` = periodic polling; `GPIO_IRQ` = edge IRQ. */
    wink_button_event_drive_t drive;

    /** Poll period in ms.
     *  - For `SOFT_POLL`: mandatory (must be > 0).
     *  - For `GPIO_IRQ`: recommended debounce follow-up period; may be 0 to
     *    let the runtime pick a sensible default derived from `debounce_ms`.
     */
    uint32_t auto_poll_ms;

    /** Debounce window in ms. 0 = disable debounce (expert mode: raw edges
     *  pass straight through). The DAL button's internal debounce state
     *  machine is retuned from this value at start(). */
    uint32_t debounce_ms;

    /** Reserved for S3: request the IRQ path to remain armed across deep
     *  sleep so a button press wakes the SoC. Currently ignored (there is
     *  no RTC-GPIO PAL API yet). */
    bool wake_from_sleep;
} wink_button_event_config_t;

/**
 * @brief Start button event dispatch for @p btn using the given @p cfg.
 *
 * @param btn  Initialised dal_button_t instance.
 * @param cfg  Pointer to a config, typically `static const` at TU scope.
 *             The struct is only referenced during this call; the
 *             implementation copies whatever it needs into the slot.
 *
 * @return WINK_OK on success.
 *         WINK_ERR_INVALID_ARG        `btn` or `cfg` NULL, or
 *                                     `SOFT_POLL` with `auto_poll_ms == 0`.
 *         WINK_ERR_INVALID_STATE      `btn` already has an event stream
 *                                     running (call stop first).
 *         WINK_ERR_RESOURCE_EXHAUSTED slot pool full.
 *         WINK_ERR_NOT_INITIALIZED    `btn` was never `dal_button_init`ed.
 *         Other codes propagated from `wink_periodic_start_ex`.
 *
 * @note S2 degrade frame: when `cfg->drive == WINK_BUTTON_DRIVE_GPIO_IRQ`
 *       this call falls back to `SOFT_POLL` internally so cross-target
 *       samples never break. S3 wires the real ESP32 IRQ path; S4 adds
 *       the trace warn + strict mode.
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_button_events_start(dal_button_t *btn,
                                       const wink_button_event_config_t *cfg);

/**
 * @brief Stop button event dispatch for @p btn.
 *
 * Idempotent: safe to call on a NULL or never-started button. Restores
 * any pre-existing `dal_button_on_event` user callback captured at
 * start(), tears down the periodic slot fully (no soft-timer slot leak),
 * and frees the events-slot for reuse.
 *
 * @param btn  Button instance (NULL-safe).
 */
void wink_button_events_stop(dal_button_t *btn);

#ifdef __cplusplus
}
#endif

#endif /* WINK_BUTTON_EVENTS_H */
