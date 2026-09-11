// SPDX-License-Identifier: Apache-2.0
// CMS8S78xx chip-private state (Stage2 S2-1, PLAN-20260911-MCS51-S2).
//
// Aggregate per-instance private block bound through Mcu51Context::soc_priv.
// Generic core NEVER includes this header (CPL-11/14); chip sources include
// it for the type + pool binder. Naming: cms8s_*/Cms8s* live here,
// mcs51_*/wink_mcs51_* only in core (two-way naming gate, §6.2 Check 2).
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "mcs51_context.h"

#ifdef __cplusplus
extern "C" {
#endif

// On-chip XDATA aperture (GAP-09, datasheet §2.2.3 — sunk from generic
// context.h in S2-2 Step 3, zero users elsewhere):
//  - CMS8S78xx: 1 KB internal XRAM (0x0000..0x03FF).
// Classic 8052 has no on-chip XRAM; external MOVX size stays the per-app
// WINK_MCS51_XDATA_SIZE knob (8 KB default, stage6 sinks it to board scope).
#define CMS8S_XRAM_SIZE 1024u
#define CMS8S_XSFR_WINDOW_BASE 0xF000u  // extended-SFR MOVX window

// TA protection window + WDT coarse model (owner: cms8s_sys.cpp).
typedef struct {
    // 0 = waiting 0xAA, 1 = got 0xAA waiting 0x55, 2 = unlocked (next
    // protected write passes and consumes the window).
    uint8_t ta_phase;
    // virtual_us when 0xAA was accepted (TA window timeout, GAP-07 coarse).
    uint64_t ta_aa_us;
    // virtual_us of the last WDT start/feed (WDTRE 0->1 or WDTCLR strobe).
    uint64_t wdt_last_feed_us;
    // Overflow already counted for the current arming (one count per
    // episode until the next feed; Release warn-once latch lives with the
    // file-static diagnostic counter).
    uint8_t wdt_overflow_latched;
} Cms8sSysState;

// Hardware buzzer generator (owner: cms8s_buzzer.cpp).
typedef struct {
    bool     running;
    uint8_t  pin_level;
    uint32_t half_period_us;
    uint64_t next_toggle_us;
    uint32_t toggle_count;
} Cms8sBuzzerState;

// On-chip 12-bit ADC + ADET trigger (owner: cms8s_adc.cpp).
typedef struct {
    uint32_t conversion_count;
    uint8_t  last_channel;
} Cms8sAdcState;

typedef struct {
    uint16_t last_pin;
    uint8_t  last_level;
    bool     have_sample;
} Cms8sAdetState;

// Aggregate chip block: ONE pool slot per context instance (BSS pool in
// cms8s_sys.cpp, indexed by ctx->instance_index). T3/T4 + capture/compare +
// port sampling join this struct in S2-2 Step 1.
typedef struct {
    Cms8sSysState   sys;
    Cms8sBuzzerState buzzer;
    Cms8sAdcState   adc;
    Cms8sAdetState  adet;
    bool            in_poll;
} Cms8sPriv;

// Idempotent per-instance bind: memset pool[idx] + point soc_priv at it.
// Called FIRST by every cms8s_*_init (self-binding: core reset never names
// chip symbols, §3.1 one-way rule). Rebind (soc_priv == slot) is a no-op;
// family switches NULL soc_priv in set_family so the next bind re-seeds.
void cms8s_soc_bind(struct Mcu51Context *ctx);

// Accessor: chip models resolve private state through soc_priv (never
// through context-inline fields since S2-1). NULL ctx = active context.
// Bind-on-demand: an unbound chip-family context (fresh, never reset/init,
// e.g. SFR-operator tests that only set_family) heals to a zeroed slot —
// exactly the old inline-zero semantics. Other families never bind here
// (callers gate on family first); a null return means "no chip state".
static inline Cms8sPriv *cms8s_priv(struct Mcu51Context *ctx) {
    if (!ctx) {
        ctx = mcs51_get_context();
    }
    if (!ctx->soc_priv && ctx->family == MCS51_FAMILY_CMS8S78XX) {
        cms8s_soc_bind(ctx);
    }
    return (Cms8sPriv *)ctx->soc_priv;
}

// Family gate for chip hook bodies (S2-1): hooks stay installed across a
// family switch (reset only overwrites same-address slots), so a stale chip
// hook firing on another family must bail neutrally instead of touching
// no chip state. Returns true when hooks may proceed.
static inline bool cms8s_hook_armed(struct Mcu51Context *ctx) {
    if (!ctx) {
        ctx = mcs51_get_context();
    }
    return ctx->family == MCS51_FAMILY_CMS8S78XX && ctx->soc_priv != 0;
}

#ifdef __cplusplus
}  // extern "C"
#endif
