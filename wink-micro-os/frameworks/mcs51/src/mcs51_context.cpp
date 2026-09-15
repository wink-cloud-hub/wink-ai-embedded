// SPDX-License-Identifier: LGPL-3.0-only
// Task R2: Mcu51Context container instance and silicon reset implementation.
#include "mcs51_context.h"
#include "mcs51_peripheral.h"
#include "mcs51_sfr_map.h"
#include "wink_mcs51_wdt.h"

#include <cassert>
#include <cstdlib>
#include <cstring>

#ifndef WINK_MCS51_STRICT
#include "pal_log.h"
#endif

static Mcu51Context s_default_mcu_context = {};
Mcu51Context* g_active_mcu_context = &s_default_mcu_context;

namespace {

// Compile-time family default (production wasm builds define WINK_MCU_*
// directory-scoped; host tests override through mcs51_context_set_family).
uint8_t s_mcu_family =
#if defined(WINK_MCU_CMS8S78XX)
    MCS51_FAMILY_CMS8S78XX;
#else
    MCS51_FAMILY_CLASSIC;
#endif

// Family-specific silicon seeds applied AFTER memset + peripheral
// init/reset (GAP-04 CKCON reset value, GAP-13 power-on Fosc). Facts come
// from the family descriptor (M1); no per-series branch here.
void apply_silicon_seeds(Mcu51Context* ctx) {
    const mcs51_family_desc_t* d = mcs51_family_desc(s_mcu_family);
    ctx->family = s_mcu_family;
    // Stage0: snapshot capabilities once; hot paths read ctx->caps_cache.
    ctx->caps_cache = d->capabilities;
    ctx->sfr_shadow[MCS51_SFR_CKCON] = d->ckcon_reset;
    if (d->fosc_hz != 0u) {
        // Fixed on-chip RC (e.g. CMS8S 24 MHz ±1%). Set the field on the
        // reset context directly (reset may target a context other than
        // the active one). The SFR-access billing quantum stays on its
        // 12 MHz-calibrated budget deliberately (ADR-0072).
        ctx->clock_hz = d->fosc_hz;
    }
    // Else (classic: board crystal): leave clock_hz at 0 so the 12 MHz
    // family default remains in effect; CKCON shadow stays 0x00
    // (counts_to_us picks the /12 divider — no CKCON on classic 8052).
}

// Review hardening (link fuse): link-time self-registration only works when
// the chip register OBJECT rides the link line — a hand-written link that
// pulls just the family archive silently loses every chip model (a
// constructor-only archive member is never pulled by the symbol scan). Fuse
// instead of bare-core degradation: STRICT aborts, release warns once and
// counts every detection for diagnostics.
uint32_t s_chip_models_missing = 0u;
#ifndef WINK_MCS51_STRICT
bool s_chip_models_missing_warned = false;
#endif

void verify_chip_models_registered(Mcu51Context* ctx) {
    const mcs51_family_desc_t* d = mcs51_family_desc(ctx->family);
    if ((d->capabilities & MCS51_CAP_CHIP_MODELS) == 0u) {
        return;  // pure-core family: an empty registry is the contract
    }
    for (uint8_t i = 0u; i < mcs51_peripheral_registered_count(); ++i) {
        if (mcs51_peripheral_active_for(mcs51_peripheral_registered(i),
                                        ctx->family)) {
            return;  // chip package present: healthy link
        }
    }
    if (s_chip_models_missing < 0xFFFFFFFFu) {
        ++s_chip_models_missing;
    }
#ifdef WINK_MCS51_STRICT
    assert(0 && "chip family selected but no peripheral registered at "
                "link-time (WINK_MCS51_STRICT)");
    std::abort();
#else
    if (!s_chip_models_missing_warned) {
        s_chip_models_missing_warned = true;
        pal_log_w("MCS51",
                  "family '%s' selected but no peripheral is registered at "
                  "link-time (chip register OBJECT missing?) - bare core",
                  d->name);
    }
#endif
}

}  // namespace

extern "C" {

void mcs51_context_set_family(uint8_t family) {
    s_mcu_family = family;
    // S2-1: unbind the old chip slot on family switch (stale pool data must
    // never leak across families). The new slot is memset + bound by the
    // first chip init (self-binding; core names no chip symbols). Classic
    // binds nullptr explicitly — zero extension, pure core.
    mcs51_get_context()->soc_priv = nullptr;
    apply_silicon_seeds(mcs51_get_context());
}

uint8_t mcs51_context_get_family(void) {
    return s_mcu_family;
}

void mcs51_context_init(Mcu51Context* ctx, uint8_t idx) {
    if (ctx == nullptr) {
        return;
    }
    // S2-1: out-of-range clamps to the last slot (unit-tested); reset
    // asserts the invariant as backstop against hand-built contexts.
    ctx->instance_index =
        (idx < MCS51_MAX_INSTANCES) ? idx : (MCS51_MAX_INSTANCES - 1u);
}

void mcs51_context_reset(Mcu51Context* ctx) {
    if (ctx == nullptr) {
        return;
    }
    // S2-1: slot fuse — a hand-built context with a wild index must fail
    // loudly, never alias another instance's pool slot.
    assert(ctx->instance_index < MCS51_MAX_INSTANCES);

    // Preserve registered ISR table, external pin baseline and interrupt trigger mode across hardware reset (world state, ADR-0076)
    void (*saved_isrs[28])(void);
    std::memcpy(saved_isrs, ctx->isr_table, sizeof(saved_isrs));

    Mcu51ExtIntLine saved_lines[2];
    std::memcpy(saved_lines, ctx->extint.lines, sizeof(saved_lines));
    uint8_t saved_tcon_it = ctx->sfr_shadow[0x88] & ((1u << 0) | (1u << 2));

    // S2-1 memset ordering iron rule: stash instance_index before the wipe,
    // restore immediately after (same idiom as saved_isrs above).
    const uint8_t saved_idx = ctx->instance_index;

    // ADR-0082 D3: Preserve reset reason and sticky PORF bit across memset
    const uint8_t reason = ctx->reset_reason ? ctx->reset_reason : ctx->last_reset_reason;
    const uint8_t saved_porf = ctx->sfr_shadow[0x97] & 0x40u;

    // ADR-0072: Slave virtual clock is monotonic across warm resets (SWRST/WDT/EXT);
    // only a cold power-on reset (POR) re-seeds virtual time to 0.
    const bool is_warm_reset = (reason != MCS51_RESET_REASON_POR && reason != MCS51_RESET_REASON_NONE);
    const uint64_t saved_virtual_us = is_warm_reset ? ctx->virtual_us : 0u;
    const uint64_t saved_slice_start_us = is_warm_reset ? ctx->slice_start_us : 0u;
    const uint32_t saved_quota_yields = is_warm_reset ? ctx->quota_yields : 0u;
    const uint32_t saved_step_count = is_warm_reset ? ctx->step_count : 0u;

    // Zero entire context memory (also clears family + §8 model states,
    // which are per-instance fields since M2 — no file-static to clean).
    std::memset(ctx, 0, sizeof(Mcu51Context));

    // Slot restore + explicit classic bind (nullptr = zero extension).
    // Chip slots are memset + bound by the first chip init below
    // (self-binding; core names no chip symbols, §3.1 one-way rule).
    ctx->instance_index = saved_idx;
    ctx->soc_priv = nullptr;
    ctx->last_reset_reason = reason;
    ctx->virtual_us = saved_virtual_us;
    ctx->slice_start_us = saved_slice_start_us;
    ctx->quota_yields = saved_quota_yields;
    ctx->step_count = saved_step_count;

    // Restore ISR table and INT0/INT1 line baseline (port sampling
    // re-baselines in the chip pool, S4-D2).
    std::memcpy(ctx->isr_table, saved_isrs, sizeof(saved_isrs));
    std::memcpy(ctx->extint.lines, saved_lines, sizeof(saved_lines));
    ctx->sfr_shadow[0x88] |= saved_tcon_it;

    // ── Silicon Reset Seeds (Task R2) ────────────────────────────────────────
    // Ports P0..P3 power on as 0xFF (quasi-bidirectional inputs, weak pull-up)
    ctx->sfr_shadow[0x80] = 0xFFu; // P0
    ctx->sfr_shadow[0x90] = 0xFFu; // P1
    ctx->sfr_shadow[0xA0] = 0xFFu; // P2
    ctx->sfr_shadow[0xB0] = 0xFFu; // P3

    // Standard 8051 SFR seeds
    ctx->sfr_shadow[0x81] = 0x07u; // SP = 0x07
    ctx->sfr_shadow[0x87] = 0x00u; // PCON = 0x00

    // S2-1: reference-rail defaults are NOT seeded here anymore (CPL-19).
    // mcs51_adc_reset() clears only the injection table; the 3000/3000
    // defaults are injected by chip reset via the generic rail parameters.

    // S2-2: pin-share selector seeds (were a 13-entry 0x7F block here,
    // CPL-19) now live with their owning models: the ADC selector in the
    // ADC model reset, INT selectors in extint reset, timer/capture
    // selectors in timer reset. Same values, same reset-loop order
    // (peripheral loop runs below).

    // Record the family on the context BEFORE peripheral init so models
    // observe a consistent family for the whole reset.
    ctx->family = s_mcu_family;

    // S4-H1 (review hardening): trap/hook registration resolves the ACTIVE
    // context, not this argument — pin it across both dispatch loops so a
    // reset targeting a non-active instance cannot bleed registrations into
    // another instance. Plain save/restore (no-exception codebase, same
    // idiom as saved_idx/saved_isrs above).
    Mcu51Context* saved_active = mcs51_get_context();
    mcs51_set_active_context(ctx);

    // Stage5 CPL-06: (re)load the per-context source->vector profile. The
    // core standard rows are written here; chip packages re-apply their
    // extended rows in the descriptor loops below (their reset installs
    // irq_map_extend, but the memset above cleared it, so this call sees a
    // pure core profile first).
    wink_mcs51_reset_irq_map();

    // Initialize peripherals via descriptor table (Task R1), filtered by
    // family (M1): series models never install hooks on another family.
    // Stage4 CPL-10: core table first, then the chip registry (same filter).
    for (uint8_t i = 0; i < g_mcs51_num_peripherals; ++i) {
        if (!mcs51_peripheral_active_for(&g_mcs51_peripherals[i], ctx->family)) {
            continue;
        }
        if (g_mcs51_peripherals[i].init != nullptr) {
            g_mcs51_peripherals[i].init(ctx);
        }
    }
    for (uint8_t i = 0; i < mcs51_peripheral_registered_count(); ++i) {
        const mcs51_peripheral_desc_t* d = mcs51_peripheral_registered(i);
        if (!mcs51_peripheral_active_for(d, ctx->family)) {
            continue;
        }
        if (d->init != nullptr) {
            d->init(ctx);
        }
    }

    // Reset peripherals via descriptor table (Task R1), same filter.
    for (uint8_t i = 0; i < g_mcs51_num_peripherals; ++i) {
        if (!mcs51_peripheral_active_for(&g_mcs51_peripherals[i], ctx->family)) {
            continue;
        }
        if (g_mcs51_peripherals[i].reset != nullptr) {
            g_mcs51_peripherals[i].reset(ctx);
        }
    }
    for (uint8_t i = 0; i < mcs51_peripheral_registered_count(); ++i) {
        const mcs51_peripheral_desc_t* d = mcs51_peripheral_registered(i);
        if (!mcs51_peripheral_active_for(d, ctx->family)) {
            continue;
        }
        if (d->reset != nullptr) {
            d->reset(ctx);
        }
    }

    mcs51_set_active_context(saved_active);

    // Family-specific silicon seeds last: CKCON reset value / power-on Fosc
    // must not be disturbed by peripheral resets (GAP-04/GAP-13).
    apply_silicon_seeds(ctx);

    // ADR-0082 D3: WDCON reset seeds and sticky flag application
    if (ctx->family == MCS51_FAMILY_CMS8S78XX) {
        if (reason == MCS51_RESET_REASON_POR || reason == MCS51_RESET_REASON_NONE) {
            // Cold power-on reset: PORF forced 1, WDTRF 0
            ctx->sfr_shadow[0x97] |= 0x40u;
            ctx->sfr_shadow[0x97] &= ~0x04u;
        } else {
            // Warm reset (SWRST, WDT, EXT): restore sticky PORF
            ctx->sfr_shadow[0x97] = (ctx->sfr_shadow[0x97] & ~0x40u) | saved_porf;
            if (reason == MCS51_RESET_REASON_WDT) {
                ctx->sfr_shadow[0x97] |= 0x04u;  // WDTRF set
            } else {
                ctx->sfr_shadow[0x97] &= ~0x04u; // WDTRF 0
            }
        }
        // SWRST and WDTCLR always reset to 0
        ctx->sfr_shadow[0x97] &= ~(0x80u | 0x01u);
    }

    // Post-reset link fuse: a chip family without any registered chip model
    // means the register OBJECT never got linked (review hardening).
    verify_chip_models_registered(ctx);
}

void wink_mcs51_trigger_reset(mcs51_reset_reason_t reason) {
    Mcu51Context* ctx = mcs51_get_context();
    if (!ctx) return;
    if (ctx->reset_guard) {
        return;
    }
    if (ctx->reset_pending) {
        return;
    }
    ctx->reset_pending = 1u;
    ctx->reset_reason = static_cast<uint8_t>(reason);
}

bool wink_mcs51_has_pending_reset(void) {
    Mcu51Context* ctx = mcs51_get_context();
    return ctx ? (ctx->reset_pending != 0u) : false;
}

mcs51_reset_reason_t wink_mcs51_get_pending_reset_reason(void) {
    Mcu51Context* ctx = mcs51_get_context();
    return ctx ? static_cast<mcs51_reset_reason_t>(ctx->reset_reason) : MCS51_RESET_REASON_NONE;
}

mcs51_reset_reason_t wink_mcs51_get_last_reset_reason(void) {
    Mcu51Context* ctx = mcs51_get_context();
    return ctx ? static_cast<mcs51_reset_reason_t>(ctx->last_reset_reason) : MCS51_RESET_REASON_NONE;
}

void wink_mcs51_clear_pending_reset(void) {
    Mcu51Context* ctx = mcs51_get_context();
    if (ctx) {
        ctx->reset_pending = 0u;
        ctx->reset_reason = 0u;
    }
}

uint32_t wink_mcs51_chip_models_missing_count(void) {
    return s_chip_models_missing;
}

} // extern "C"
