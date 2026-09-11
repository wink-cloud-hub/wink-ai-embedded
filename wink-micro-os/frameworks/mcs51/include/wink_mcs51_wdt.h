// SPDX-License-Identifier: Apache-2.0
// Stage 3 Task 4 (A-04, GAP-07): CMS8S78xx watchdog coarse model + TA window.
//
// WDT: overflow interval from CKCON.WTS (vendor wdt.h: 2^17..2^22,2^24,2^26
// Tsys) against the hardware clock; checked in the microstep poll path and
// via an explicit test seam. STRICT aborts at first overflow, Release counts
// once per arming episode (GAP-10 runner verdict) and keeps running — a full
// simulated chip reset needs fiber/main re-entry and stays future work.
//
// TA: 0xAA timestamp + invalidation on any intervening firmware SFR write
// (bridge notifies before hook dispatch); protected writes still roll back
// when locked. Window/timeout values are coarse (documented, not silicon).
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Mcu51Context;

// WDT overflow interval in virtual us from live CKCON.WTS + hardware clock.
// 0 when the family has no WDT (classic) or the clock reads 0.
uint64_t wink_mcs51_wdt_interval_us(void);

// Poll entry: compare virtual_us against last feed + interval; on overflow
// STRICT aborts, Release counts once per episode. Called from the cms8s_sys
// microstep poll and from tests (after test_advance_virtual_us).
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

// Bridge hook: firmware issued an SFR write to `addr` (called BEFORE the
// per-address hook dispatch). Clears a half-open TA window on intervening
// writes. No-op on families without TA protection.
void cms8s_sys_notify_sfr_write(struct Mcu51Context* ctx, uint8_t addr);

#ifdef __cplusplus
}  // extern "C"
#endif
