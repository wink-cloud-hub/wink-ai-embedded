// SPDX-License-Identifier: Apache-2.0
// Generic watchdog coarse model + timed-access (TA) window narrowing + Reset Controller (ADR-0082).
//
// WDT: overflow interval from the clock-control WTS field against the
// hardware clock; checked in the owning chip's microstep poll path and via
// an explicit test seam. STRICT aborts at first overflow, Release latches
// the reset state machine, logs once per arming episode (GAP-10 runner verdict),
// and triggers cooperative fiber / main re-entry.
//
// TA: 0xAA timestamp + invalidation on any intervening firmware SFR write
// (the pre-dispatch notify runs before hook dispatch); protected writes
// still roll back when locked. Window/timeout values are coarse
// (documented, not silicon).
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Mcu51Context;

// Reset source classifications (ADR-0082 D3)
typedef enum {
    MCS51_RESET_REASON_NONE     = 0,
    MCS51_RESET_REASON_POR      = 1,
    MCS51_RESET_REASON_SOFTWARE = 2,
    MCS51_RESET_REASON_WDT      = 3,
    MCS51_RESET_REASON_EXT      = 4,
} mcs51_reset_reason_t;

// WDT overflow interval in virtual us from live CKCON.WTS + hardware clock.
// 0 when the family has no WDT (classic) or the clock reads 0.
uint64_t wink_mcs51_wdt_interval_us(void);

// Poll entry: compare virtual_us against last feed + interval; on overflow
// STRICT aborts, Release counts once per episode and latches WDT reset.
// Called from the owning chip's microstep poll and from tests.
void wink_mcs51_wdt_check(void);

// Next WDT deadline (last feed + interval) or UINT64_MAX when disabled or
// already latched. Used by the low-power next-event aggregation.
uint64_t wink_mcs51_wdt_next_event_us(struct Mcu51Context* ctx);

// Last feed/start timestamp (test observability).
uint64_t wink_mcs51_wdt_last_feed_us(void);

#ifdef __EMSCRIPTEN__
__attribute__((used))
#endif
// GAP-10 firmware-health verdict: saturating overflow episode count.
uint32_t wink_mcs51_wdt_overflow_total(void);

// ── Reset Controller Interfaces (ADR-0082) ───────────────────────────────────
void wink_mcs51_trigger_reset(mcs51_reset_reason_t reason);
bool wink_mcs51_has_pending_reset(void);
mcs51_reset_reason_t wink_mcs51_get_pending_reset_reason(void);
mcs51_reset_reason_t wink_mcs51_get_last_reset_reason(void);
void wink_mcs51_clear_pending_reset(void);
void wink_mcs51_test_run_reentry_loop(void (*fn)(void), uint32_t max_boots);


#ifdef __cplusplus
}  // extern "C"
#endif

