/**
 * @file wink_button_events_internal.h
 * @brief BAL-private header shared between wink_button_events.c and
 *        wink_button_events_irq.c (S3).
 *
 * NOT installed. Not part of the public BAL API. Lives in src/ so the
 * layering guard for bal/include public headers does not reach it. This
 * is the canonical place for the slot struct definition, the s_slots
 * array, and the internal helper prototypes that the IRQ backend needs
 * to reach (arm/disarm/dispatch_stable/find_slot).
 *
 * Copyright (c) 2026 Wink-AI.
 */
#ifndef WINK_BUTTON_EVENTS_INTERNAL_H
#define WINK_BUTTON_EVENTS_INTERNAL_H

#include "input/wink_button_events.h"
#include "wink_tasks.h"
#include "dal_button.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── per-button slot (shared between poll + IRQ backends) ─────
 * A slot with btn == NULL is free. BSS zero-init gives btn=NULL. */
typedef struct {
    dal_button_t              *btn;
    wink_periodic_handle_t     period_h;      /* SOFT_POLL: periodic tick handle */
    dal_button_event_cb        orig_cb;
    void                      *orig_cb_ctx;
    wink_button_event_drive_t  drive;         /* Which backend armed this slot */
    uint32_t                   debounce_ms;   /* Cached from cfg (IRQ debounce) */
    uint32_t                   long_press_ms; /* Cached from cfg (IRQ long-press) */

    /* IRQ-only state (unused for SOFT_POLL — kept as void ptr / int32
     * to avoid ESP_PLATFORM ifdefs in the struct definition itself).
     * irq_debounce_h   : one-shot soft-timer handle armed by the daemon on
     *                    each pending edge; on expiry we sample stable pin
     *                    and dispatch PRESS/RELEASE.
     * irq_longpress_h  : one-shot soft-timer armed AFTER a confirmed press;
     *                    on expiry we dispatch LONG_PRESS (if still held).
     * last_pressed     : cached stable state from the last debounce
     *                    dispatch — the state machine baseline. */
    int32_t                    irq_debounce_h;
    int32_t                    irq_longpress_h;
    bool                       last_pressed;
    bool                       long_press_fired;
} button_event_slot_t;

/* Slot pool visible to both TUs. */
extern button_event_slot_t g_button_event_slots[WINK_BUTTON_EVENTS_MAX];

/* ── Internal helpers (shared) ─────────────────────────────── */

/* Find slot index currently tracking @p btn, or -1 if not tracked. */
int wink_button_events_find_slot(const dal_button_t *btn);

/**
 * @brief Runs the PRESS/RELEASE/LONG_PRESS state machine for one slot given
 *        a freshly-sampled stable pressed level.
 *
 * Called from BOTH:
 *   - poll_tick path: after dal_button_poll updated stable state.
 *   - IRQ debounce timeout: after the daemon woke and stable pin was read.
 *
 * Long-press handling:
 *   - PRESS transition: (re-)arm the slot's long-press one-shot for
 *     `long_press_ms` if a timer handle is available.
 *   - RELEASE transition: stop the long-press one-shot.
 *
 * @param s              Slot pointer (must be allocated, btn != NULL).
 * @param stable_pressed Freshly-sampled stable pressed state.
 * @param now_ms         Current pal_os_get_ms() timestamp (caller passes).
 */
void wink_button_events_dispatch_stable(button_event_slot_t *s,
                                        bool stable_pressed,
                                        uint64_t now_ms);

/**
 * @brief Called from the IRQ backend's long-press one-shot timer.
 *
 * Dispatches WINK_EVENT_BUTTON_LONG_PRESS if the slot is still held
 * (last_pressed == true) and long_press_fired is not yet set.
 */
void wink_button_events_dispatch_long_press(button_event_slot_t *s);

/* ── IRQ backend hooks (implemented in wink_button_events_irq.c) ── */

/**
 * @brief Whether the current build supports the GPIO-IRQ button backend.
 *        Declared publicly in wink_button_events.h; also visible here.
 */
/* bool wink_button_events_irq_supported(void); -- in public header */

/**
 * @brief Arm the IRQ backend for @p slot using @p cfg.
 *
 * On success:
 *   - slot->drive is set to WINK_BUTTON_DRIVE_GPIO_IRQ.
 *   - Debounce + long-press soft timers created.
 *   - DAL event backend set to IRQ; shared GPIO ISR enabled.
 *   - The daemon is running (lazy-created on first arm).
 *
 * @return WINK_OK on success; on failure, no side-effects visible to caller
 *         (the caller can safely fall back to SOFT_POLL).
 */
WINK_WARN_UNUSED_RESULT
wink_status_t wink_button_events_irq_arm(button_event_slot_t *slot,
                                         const wink_button_event_config_t *cfg);

/**
 * @brief Disarm the IRQ backend for @p slot.
 *
 * Idempotent. Stops/destroys timers, clears DAL event backend, disables
 * shared ISR (refcount drops in DAL). Does not touch orig_cb or period_h;
 * the caller (wink_button_disable_events) handles those.
 */
void wink_button_events_irq_disarm(button_event_slot_t *slot);

#ifdef __cplusplus
}
#endif

#endif /* WINK_BUTTON_EVENTS_INTERNAL_H */
