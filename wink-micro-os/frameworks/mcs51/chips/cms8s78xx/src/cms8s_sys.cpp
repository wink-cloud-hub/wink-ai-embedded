// SPDX-License-Identifier: LGPL-3.0-only
// CMS8S78xx system-protection model:
//   - TA (Time Access) protection window for CLKDIV / WDCON writes. The
//     silicon ignores writes to protected SFRs unless immediately preceded
//     by TA = 0xAA; TA = 0x55; (ref manual §4.2); firmware must unlock every
//     write. The model reverts shadow updates that arrive locked so that
//     silicon-incorrect code fails in simulation instead of silently
//     "working" against a permissive register file. GAP-07 coarse extras:
//     0xAA carries a virtual-time stamp (stale windows expire) and any
//     intervening firmware SFR write (bridge notify) aborts a half-open
//     window, so sloppy TA sequences fail instead of passing.
//   - CLKDIV write hook: derives the simulated system clock from the
//     CMS8S78xx 24 MHz internal RC (Fsys = Fosc for div=0, else Fosc/(2*div)).
//   - Stage5 CPL-06/08 chip-wide glue: extended IRQ profile extension +
//     Timer2 multi-flag predicate + declared-XSFR allowlist validation,
//     installed per context by init/reset (see the glue section below).
//   - WDCON register exists (TA-protected) so WDT enable/feed sequences are
//     exercisable; Stage 3 Task 4 (A-04) adds the coarse overflow model:
//     CKCON.WTS interval (vendor wdt.h counts) vs virtual time since the
//     last WDTCLR feed. STRICT aborts at first overflow, Release counts once
//     per arming episode for the GAP-10 runner verdict. A full simulated
//     chip reset (context reset + main re-entry) stays future work: the
//     safety property under test is "longest blocking section < WDT
//     interval", which the counter already decides.
#include "mcs51_context.h"
#include "mcs51_family.h"
#include "mcs51_trap.h"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_isr.h"
#include "wink_mcs51_strict.h"
#include "wink_mcs51_timer.h"
#include "wink_mcs51_wdt.h"
#include "cms8s_priv.h"
#include "cms8s_sfr_map.h"
#include "cms8s_xsfr_allowlist.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#ifndef WINK_MCS51_STRICT
#include "pal_log.h"
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace {

constexpr uint8_t SFR_TA     = 0x96;
constexpr uint8_t SFR_CLKDIV = 0x8F;
constexpr uint8_t SFR_WDCON  = 0x97;
constexpr uint8_t SFR_CKCON  = 0x8E;

// GAP-24: CMS8S78xx IAP/Flash block (PCRCD @ 0xF9-0xFA, MLOCK @ 0xFB,
// MADR @ 0xFC-0xFD, MDATA @ 0xFE, MCTRL @ 0xFF). Firmware that declares
// these SFRs locally (e.g. `sfr MCTRL = 0xFF;`) would otherwise land
// silently in the shadow with no persistence or program/erase model.
// Both directions trap: status polling (`while (MCTRL & BUSY);`) reads
// shadow 0x00 (idle, terminates) but stays visible via the trigger count.
constexpr uint8_t SFR_IAP_FIRST = 0xF9u;
constexpr uint8_t SFR_IAP_LAST  = 0xFFu;

constexpr uint8_t TA_KEY1 = 0xAAu;
constexpr uint8_t TA_KEY2 = 0x55u;

constexpr uint8_t WDCON_SWRST  = 0x80u;  // WDCON.7: software reset
constexpr uint8_t WDCON_PORF   = 0x40u;  // WDCON.6: power-on reset flag
constexpr uint8_t WDCON_WDTIF  = 0x08u;  // WDCON.3: watchdog interrupt flag
constexpr uint8_t WDCON_WDTRF  = 0x04u;  // WDCON.2: watchdog reset flag
constexpr uint8_t WDCON_WDTRE  = 0x02u;  // WDCON.1: watchdog reset enable
constexpr uint8_t WDCON_WDTCLR = 0x01u;  // WDCON.0: watchdog clear (feed)

// GAP-07 coarse TA window: back-to-back TA=AA/55 + protected write spans
// ~2 microsteps (~10 us virtual). Anything with a real delay between the
// keys (e.g. delay_ms) is sloppy on silicon and must fail here. Generous on
// purpose: this is a tripwire, not a cycle model.
constexpr uint64_t kTaWindowUs = 100u;

// WDT overflow counts in Tsys per CKCON.WTS (vendor StdDriver wdt.h — note
// the gaps: WTS=6 is 2^24, WTS=7 is 2^26, not 2^23/2^25).
constexpr uint32_t kWdtCounts[8] = {
    131072u,     // 2^17
    262144u,     // 2^18
    524288u,     // 2^19
    1048576u,    // 2^20
    2097152u,    // 2^21
    4194304u,    // 2^22
    16777216u,   // 2^24
    67108864u,   // 2^26
};

// Process-level diagnostic counter (M4): silicon state stays per-context,
// counters stay file-static like UART-notready. STRICT aborts before
// counting, so the counter stays 0 there by design.
uint32_t s_wdt_triggered = 0u;
#ifndef WINK_MCS51_STRICT
bool s_wdt_warned = false;
#endif

inline bool has_wdt(const Mcu51Context* ctx) {
    // S3-H4: WDT presence reads the v2 descriptor SSOT (wdt_present) — never
    // inferred from the XSFR window (the window answers addressing, not
    // features; see mcs51_family_has_xsfr's remaining window users).
    return mcs51_family_desc(ctx->family)->wdt_present;
}

inline bool wdt_enabled(const Mcu51Context* ctx) {
    return (ctx->sfr_shadow[SFR_WDCON] & WDCON_WDTRE) != 0u;
}

inline bool wdt_counting(const Mcu51Context* ctx) {
    return ((ctx->sfr_shadow[SFR_WDCON] & WDCON_WDTRE) != 0u) ||
           ((ctx->sfr_shadow[CMS8S_SFR_EIE2] & (1u << 5)) != 0u);
}

// M2: TA phase lives in the chip pool (was Mcu51Context::sysProt, was
// file-static s_sys before that). Hooks already carry ctx; reset_state
// takes it explicitly.
void reset_state(Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    cms8s_priv(ctx)->sys.ta_phase = 0;
    cms8s_priv(ctx)->sys.ta_aa_us = 0;
    cms8s_priv(ctx)->sys.wdt_last_feed_us = 0;
    cms8s_priv(ctx)->sys.wdt_overflow_latched = 0;
    s_wdt_triggered = 0u;
#ifndef WINK_MCS51_STRICT
    s_wdt_warned = false;
#endif
}

// Returns true exactly once after a well-formed TA unlock sequence.
bool consume_unlock(Mcu51Context* ctx) {
    const bool ok = (cms8s_priv(ctx)->sys.ta_phase == 2u);
    cms8s_priv(ctx)->sys.ta_phase = 0u;
    return ok;
}

uint64_t wdt_interval_us_impl(const Mcu51Context* ctx) {
    if (!has_wdt(ctx)) {
        return 0u;
    }
    const uint32_t fsys = wink_mcs51_get_clock_hz();
    if (fsys == 0u) {
        return 0u;
    }
    const uint8_t wts = static_cast<uint8_t>((ctx->sfr_shadow[SFR_CKCON] >> 5u) & 0x07u);
    const uint64_t counts = kWdtCounts[wts];
    return (counts * 1000000ull) / fsys;
}

void wdt_overflow_policy(void) {
#ifdef WINK_MCS51_STRICT
    assert(0 && "WDT overflow: feed interval exceeded (WINK_MCS51_STRICT)");
    std::abort();
#else
    if (s_wdt_triggered < 0xFFFFFFFFu) {
        ++s_wdt_triggered;
    }
    if (!s_wdt_warned) {
        s_wdt_warned = true;
        pal_log_w("MCS51", "WDT overflow: no feed within interval");
    }
#endif
}

void wdt_check_impl(Mcu51Context* ctx) {
    if (!has_wdt(ctx) || !wdt_counting(ctx)) {
        return;
    }
    const uint64_t interval = wdt_interval_us_impl(ctx);
    if (interval == 0u) {
        return;
    }
    const uint64_t now = ctx->virtual_us;
    if (now < cms8s_priv(ctx)->sys.wdt_last_feed_us) {
        return;  // clock reset race: never report a negative age
    }
    if (now - cms8s_priv(ctx)->sys.wdt_last_feed_us >= interval) {
        const bool ie_enabled = (ctx->sfr_shadow[CMS8S_SFR_EIE2] & (1u << 5)) != 0u;
        const bool re_enabled = (ctx->sfr_shadow[SFR_WDCON] & WDCON_WDTRE) != 0u;

        if (re_enabled) {
            // ADR-0082 D4: Reset has highest priority over IRQ, suppresses Vector 20 dispatch
            if (cms8s_priv(ctx)->sys.wdt_overflow_latched == 0u) {
                cms8s_priv(ctx)->sys.wdt_overflow_latched = 1u;
                wdt_overflow_policy();
                wink_mcs51_trigger_reset(MCS51_RESET_REASON_WDT);
            }
        } else if (ie_enabled) {
            ctx->sfr_shadow[SFR_WDCON] |= WDCON_WDTIF;
            mcs51_raise_irq(IRQ_SOURCE_WDT);
            const uint64_t elapsed = now - cms8s_priv(ctx)->sys.wdt_last_feed_us;
            const uint64_t periods = elapsed / interval;
            cms8s_priv(ctx)->sys.wdt_last_feed_us += periods * interval;
        }
    }
}

// M3: C language linkage — this address is stored in the C-ABI
// mcs51_sfr_write_hook_t table (internal linkage via the enclosing
// anonymous namespace is kept).
extern "C" void on_ta_write(Mcu51Context* ctx, uint8_t addr, uint8_t old_val, uint8_t new_val) {
    if (!ctx) ctx = mcs51_get_context();
    if (!cms8s_hook_armed(ctx)) return;  // S2-1: stale hook on another family
    (void)addr;
    (void)old_val;
    // Stale half-open window expires before judging the new key.
    if (cms8s_priv(ctx)->sys.ta_phase == 1u &&
        ctx->virtual_us - cms8s_priv(ctx)->sys.ta_aa_us > kTaWindowUs) {
        cms8s_priv(ctx)->sys.ta_phase = 0u;
    }
    if (cms8s_priv(ctx)->sys.ta_phase == 0u && new_val == TA_KEY1) {
        cms8s_priv(ctx)->sys.ta_phase = 1u;
        cms8s_priv(ctx)->sys.ta_aa_us = ctx->virtual_us;
    } else if (cms8s_priv(ctx)->sys.ta_phase == 1u && new_val == TA_KEY2) {
        cms8s_priv(ctx)->sys.ta_phase = 2u;
    } else {
        // Any wrong/extra TA write aborts the sequence.
        cms8s_priv(ctx)->sys.ta_phase = 0u;
    }
}

extern "C" void on_clkdiv_write(Mcu51Context* ctx, uint8_t addr, uint8_t old_val, uint8_t new_val) {
    if (!ctx) ctx = mcs51_get_context();
    if (!cms8s_hook_armed(ctx)) return;  // S2-1: stale hook on another family
    if (!consume_unlock(ctx)) {
        // Locked write is ignored by silicon: restore the previous value.
        ctx->sfr_shadow[addr] = old_val;
        return;
    }
    // Ref manual §4.2.1: div=0 -> Fsys = Fosc; otherwise Fsys = Fosc/(2*div).
    const uint32_t fosc = mcs51_family_desc(ctx->family)->fosc_hz;
    const uint32_t fsys = (new_val == 0u)
        ? fosc
        : fosc / (2u * static_cast<uint32_t>(new_val));
    wink_mcs51_set_hardware_clock_hz(fsys != 0u ? fsys : fosc);
    // Timers already pending at the old rate must be re-based immediately.
    wink_mcs51_timers_step_to(ctx->virtual_us);
}

extern "C" void on_wdcon_write(Mcu51Context* ctx, uint8_t addr, uint8_t old_val, uint8_t new_val) {
    if (!ctx) ctx = mcs51_get_context();
    if (!cms8s_hook_armed(ctx)) return;  // S2-1: stale hook on another family

    const bool unlocked = consume_unlock(ctx);
    uint8_t effective_val = old_val;

    // ADR-0082 D3 / P06: Bit 6 (PORF) requires NO TA. Firmware write 0 clears; write 1 is ignored.
    if ((new_val & WDCON_PORF) == 0u) {
        effective_val &= ~WDCON_PORF;
    }

    if (unlocked) {
        // Bit 7: SWRST (TA-protected) — 0->1 edge triggers software reset, self-clears to 0.
        if ((old_val & WDCON_SWRST) == 0u && (new_val & WDCON_SWRST) != 0u) {
            effective_val &= ~WDCON_SWRST;
            wink_mcs51_trigger_reset(MCS51_RESET_REASON_SOFTWARE);
        } else if ((new_val & WDCON_SWRST) == 0u) {
            effective_val &= ~WDCON_SWRST;
        }

        // Bit 3: WDTIF (TA-protected) — write 0 clears; write 1 cannot set.
        if ((new_val & WDCON_WDTIF) == 0u) {
            effective_val &= ~WDCON_WDTIF;
        }

        // Bit 2: WDTRF (TA-protected) — write 0 clears; write 1 cannot set.
        if ((new_val & WDCON_WDTRF) == 0u) {
            effective_val &= ~WDCON_WDTRF;
        }

        // Bit 1: WDTRE (TA-protected) — R/W. 0->1 re-arms feed timestamp.
        const bool was_re = (old_val & WDCON_WDTRE) != 0u;
        const bool now_re = (new_val & WDCON_WDTRE) != 0u;
        if (now_re) {
            effective_val |= WDCON_WDTRE;
            if (!was_re) {
                cms8s_priv(ctx)->sys.wdt_last_feed_us = ctx->virtual_us;
                cms8s_priv(ctx)->sys.wdt_overflow_latched = 0u;
            }
        } else {
            effective_val &= ~WDCON_WDTRE;
        }

        // Bit 0: WDTCLR (TA-protected) — write 1 feeds and self-clears to 0.
        if ((new_val & WDCON_WDTCLR) != 0u) {
            cms8s_priv(ctx)->sys.wdt_last_feed_us = ctx->virtual_us;
            cms8s_priv(ctx)->sys.wdt_overflow_latched = 0u;
            effective_val &= ~WDCON_WDTCLR;
        }
    }

    ctx->sfr_shadow[addr] = effective_val;
}

// GAP-24 IAP/Flash trap (MCS51_FEAT_IAP_FLASH): the shadow keeps the value
// (session-persistent, like every SFR) but there is no program/erase timing
// or power-loss model, so every firmware-issued access is reported instead
// of succeeding silently. STRICT aborts; Release counts per access.
extern "C" void on_iap_write(Mcu51Context* ctx, uint8_t addr, uint8_t old_val, uint8_t new_val) {
    if (!ctx) ctx = mcs51_get_context();
    if (!cms8s_hook_armed(ctx)) return;  // S2-1: classic has no IAP block
    (void)addr;
    (void)old_val;
    (void)new_val;
    wink_mcs51_unsupported(MCS51_FEAT_IAP_FLASH, "IAP/Flash program/erase (MCTRL/MDATA/MADR/MLOCK/PCRCD)");
}

extern "C" void on_iap_read(Mcu51Context* ctx, uint8_t addr) {
    if (!ctx) ctx = mcs51_get_context();
    if (!cms8s_hook_armed(ctx)) return;  // S2-1: classic has no IAP block
    (void)addr;
    wink_mcs51_unsupported(MCS51_FEAT_IAP_FLASH, "IAP/Flash status poll (MCTRL/MDATA/MADR/MLOCK/PCRCD)");
}

// ── Stage5 CPL-06/08: chip-wide IRQ profile + XSFR window validation ───────
// The generic core keeps only the standard 0..5 sources plus a family-gated
// MOVX window; the proprietary knowledge is owned here:
//   * extended source->vector rows, loaded through the existing
//     wink_mcs51_set_irq_map_entry API and re-applied on every state reset
//     via ctx->irq_map_extend;
//   * the Timer2 multi-flag predicate (CCxIF/T2EXIF can be set while T2F is
//     0 — the S4-2 timer model raises the shared TIMER2 source);
//   * the GAP-23 declared-XSFR allowlist the core's unmodeled tripwire
//     consults through ctx->xsfr_validate.
// Installed by cms8s_sys init/reset (the chip descriptor every context
// reset runs), so the profile follows the CONTEXT — a classic context can
// never inherit it (L1 insulation).
struct Cms8sIrqExtension {
    mcs51_irq_source_t    src;
    mcs51_irq_map_entry_t entry;
};

// Priority bits follow the vendor IRQ_SET_PRIORITY macro with the
// en_Priority_Module enum (module = vector + 1): IP bit=module (<8),
// EIP1 bit=module-8 (8..15), EIP2 bit=module-16 (16..23), EIP3
// bit=module-24 (24..31). UART1 is absent by silicon (no UART1 here).
const Cms8sIrqExtension kCms8sIrqExtensions[] = {
    { IRQ_SOURCE_ADC,    { 19u, 0xAAu, 4u, 0xB2u, 4u, 0xBAu, 4u, MCS51_IRQ_SW_CLEAR } },
    { IRQ_SOURCE_PWM,    { 18u, 0xAAu, 3u, 0xB2u, 3u, 0xBAu, 3u, MCS51_IRQ_SW_CLEAR } },
    { IRQ_SOURCE_I2C,    { 21u, 0xAAu, 6u, 0xB2u, 6u, 0xBAu, 6u, MCS51_IRQ_SW_CLEAR } },
    { IRQ_SOURCE_SPI,    { 22u, 0xAAu, 7u, 0xB2u, 7u, 0xBAu, 7u, MCS51_IRQ_SW_CLEAR } },
    { IRQ_SOURCE_TIMER3, { 15u, 0xAAu, 0u, 0xB2u, 0u, 0xBAu, 0u, MCS51_IRQ_HW_AUTO_CLEAR } },
    { IRQ_SOURCE_TIMER4, { 16u, 0xAAu, 1u, 0xB2u, 1u, 0xBAu, 1u, MCS51_IRQ_HW_AUTO_CLEAR } },
    { IRQ_SOURCE_ACMP,   { 14u, 0xFFu, 0u, 0xFFu, 0u, 0xB9u, 7u, MCS51_IRQ_SW_CLEAR } },
    { IRQ_SOURCE_WDT,    { 20u, 0xAAu, 5u, 0x97u, 3u, 0xBAu, 5u, MCS51_IRQ_SW_CLEAR } },
};

// Loaded by the glue installer below and re-invoked by
// wink_mcs51_reset_irq_map on every state reset (mcs51_isr.cpp calls this
// through ctx->irq_map_extend). Rows are written DIRECTLY into the passed
// ctx (S5-H2 review): wink_mcs51_set_irq_map_entry is active-context-bound,
// so using it here would bleed a background context's profile into the
// active one; the public API remains the runtime override surface.
extern "C" void cms8s_irq_map_extend(struct Mcu51Context* ctx) {
    if (ctx == nullptr) {
        ctx = mcs51_get_context();
    }
    if (ctx == nullptr) {
        return;
    }
    const size_t n = sizeof(kCms8sIrqExtensions) /
                     sizeof(kCms8sIrqExtensions[0]);
    for (size_t i = 0; i < n; ++i) {
        ctx->irq_map[kCms8sIrqExtensions[i].src] =
            kCms8sIrqExtensions[i].entry;
    }
}

// S5-1 Step 1b: Timer2 multi-flag validity — active when ANY flag enabled
// in T2IE is set (T2F bit7, T2EXIF bit6, T2C3..0IF bits 3:0). Every other
// source keeps the standard single-bit flag check.
extern "C" bool cms8s_irq_flag_predicate(struct Mcu51Context* ctx,
                                         mcs51_irq_source_t src,
                                         const mcs51_irq_map_entry_t* entry) {
    if (ctx == nullptr) {
        ctx = mcs51_get_context();
    }
    if (ctx == nullptr || entry == nullptr) {
        return false;
    }
    if (src == IRQ_SOURCE_TIMER2 && entry->flag_sfr == CMS8S_SFR_T2IF) {
        return ((ctx->sfr_shadow[CMS8S_SFR_T2IF] &
                 ctx->sfr_shadow[CMS8S_SFR_T2IE]) != 0);
    }
    if (src == IRQ_SOURCE_PWM) {
        constexpr uint16_t XSFR_PWMZIF = 0xF16Du;
        constexpr uint16_t XSFR_PWMZIE = 0xF169u;
        constexpr uint16_t XSFR_PWMPIF = 0xF16Cu;
        constexpr uint16_t XSFR_PWMPIE = 0xF168u;
        constexpr uint16_t XSFR_PWMUIF = 0xF16Eu;
        constexpr uint16_t XSFR_PWMUIE  = 0xF16Au;
        constexpr uint16_t XSFR_PWMDIF  = 0xF16Fu;
        constexpr uint16_t XSFR_PWMDIE  = 0xF16Bu;
        constexpr uint16_t XSFR_PWMFBKC = 0xF166u;
        uint8_t z = ctx->xdata_shadow[XSFR_PWMZIF] & ctx->xdata_shadow[XSFR_PWMZIE];
        uint8_t p = ctx->xdata_shadow[XSFR_PWMPIF] & ctx->xdata_shadow[XSFR_PWMPIE];
        uint8_t u = ctx->xdata_shadow[XSFR_PWMUIF] & ctx->xdata_shadow[XSFR_PWMUIE];
        uint8_t d = ctx->xdata_shadow[XSFR_PWMDIF] & ctx->xdata_shadow[XSFR_PWMDIE];
        uint8_t fb = ((ctx->xdata_shadow[XSFR_PWMFBKC] & 0x40u) && (ctx->xdata_shadow[XSFR_PWMFBKC] & 0x80u)) ? 1u : 0u;
        return (z | p | u | d | fb) != 0;
    }
    if (entry->flag_sfr == 0xFFu) {
        return true;
    }
    return (ctx->sfr_shadow[entry->flag_sfr] &
            (1u << entry->flag_bit)) != 0;
}

// GAP-23 declared-XSFR membership (moved from the generic xdata TU in
// stage5 CPL-08): binary search over the audit-generated allowlist. The
// ctx parameter is part of the hook ABI (S5-H2 review); this part has a
// static declared set, so it carries no per-context state.
extern "C" bool cms8s_xsfr_allowlisted(struct Mcu51Context* ctx,
                                       uint64_t addr) {
    (void)ctx;
    if (addr > 0xFFFFull) {
        return false;
    }
    const uint16_t a = static_cast<uint16_t>(addr);
    const uint16_t* first = kMcs51XsfrAllowlist;
    return std::binary_search(first, first + kMcs51XsfrAllowlistCount, a);
}

}  // namespace

extern "C" {

// S3-2: pre-dispatch notify lives below (same TU); forward-declared for the
// install helper. The wink_mcs51_wdt.h hard export is gone (CPL-14) — this TU
// is the only referrer.
void cms8s_sys_notify_sfr_write(struct Mcu51Context* ctx, uint8_t addr);

// S4-H2 (reset-rebuilds-registration contract): TA/WDCON/IAP dispatch
// installation shared by init and reset (trap_register overwrites the same
// slots — idempotent; the on_* hooks resolve via the file namespace).
static void install_dispatch(struct Mcu51Context* ctx) {
    // S3-2: install the pre-dispatch notify with the explicit ctx pointer
    // (per-context by construction, no active-context dependence). The bridge
    // runs this before the per-address hooks below (GAP-07 ordering: TA
    // half-open windows abort on intervening firmware writes first).
    ctx->sfr_write_notify = cms8s_sys_notify_sfr_write;
    mcs51_trap_register_sfr_write(SFR_TA, on_ta_write);
    mcs51_trap_register_sfr_write(SFR_CLKDIV, on_clkdiv_write);
    mcs51_trap_register_sfr_write(SFR_WDCON, on_wdcon_write);
    // GAP-24: IAP/Flash block, both directions (see on_iap_* above).
    for (uint16_t a = SFR_IAP_FIRST; a <= SFR_IAP_LAST; ++a) {
        const uint8_t addr = static_cast<uint8_t>(a);
        mcs51_trap_register_sfr_write(addr, on_iap_write);
        mcs51_trap_register_sfr_read(addr, on_iap_read);
    }
}

// Stage5 CPL-06/08 (S4-H2 discipline: reset rebuilds every registration):
// chip-wide IRQ profile extension + XSFR validation hooks, per-context by
// construction (memset zero on classic/foreign contexts = standard path).
static void install_irq_bus_glue(struct Mcu51Context* ctx) {
    if (ctx->family != MCS51_FAMILY_CMS8S78XX) {
        return;  // defensive: standalone call on a foreign context
    }
    ctx->irq_map_extend = cms8s_irq_map_extend;
    ctx->irq_flag_predicate = cms8s_irq_flag_predicate;
    ctx->xsfr_validate = cms8s_xsfr_allowlisted;
    cms8s_irq_map_extend(ctx);  // load now; state resets re-apply via hook
}

void cms8s_sys_reset(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    if (!ctx) return;
    cms8s_soc_bind(ctx);  // defensive: standalone resets bind too (no-op if bound)
    reset_state(ctx);
    install_dispatch(ctx);
    install_irq_bus_glue(ctx);
}

void cms8s_sys_init(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    if (!ctx) return;
    cms8s_soc_bind(ctx);  // bind BEFORE any pool deref (ordering invariant)
    reset_state(ctx);
    install_dispatch(ctx);
    install_irq_bus_glue(ctx);
}

void cms8s_sys_poll(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    if (!cms8s_hook_armed(ctx)) return;  // review hardening: unbound/classic
    wdt_check_impl(ctx);
}

uint64_t cms8s_sys_next_event_us(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    if (!has_wdt(ctx) || !wdt_counting(ctx)) {
        return UINT64_MAX;
    }
    const bool ie_enabled = (ctx->sfr_shadow[CMS8S_SFR_EIE2] & (1u << 5)) != 0u;
    const bool re_enabled = (ctx->sfr_shadow[SFR_WDCON] & WDCON_WDTRE) != 0u;
    if (re_enabled && !ie_enabled && cms8s_priv(ctx)->sys.wdt_overflow_latched != 0u) {
        return UINT64_MAX;
    }
    const uint64_t interval = wdt_interval_us_impl(ctx);
    if (interval == 0u) {
        return UINT64_MAX;
    }
    return cms8s_priv(ctx)->sys.wdt_last_feed_us + interval;
}

uint64_t wink_mcs51_wdt_interval_us(void) {
    return wdt_interval_us_impl(mcs51_get_context());
}

void wink_mcs51_wdt_check(void) {
    Mcu51Context* ctx = mcs51_get_context();
    if (!cms8s_hook_armed(ctx)) return;  // review hardening: unbound/classic
    wdt_check_impl(ctx);
}

uint64_t wink_mcs51_wdt_next_event_us(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    if (!has_wdt(ctx) || !wdt_counting(ctx)) {
        return UINT64_MAX;
    }
    const bool ie_enabled = (ctx->sfr_shadow[CMS8S_SFR_EIE2] & (1u << 5)) != 0u;
    const bool re_enabled = (ctx->sfr_shadow[SFR_WDCON] & WDCON_WDTRE) != 0u;
    if (re_enabled && !ie_enabled && cms8s_priv(ctx)->sys.wdt_overflow_latched != 0u) {
        return UINT64_MAX;
    }
    const uint64_t interval = wdt_interval_us_impl(ctx);
    if (interval == 0u) {
        return UINT64_MAX;
    }
    return cms8s_priv(ctx)->sys.wdt_last_feed_us + interval;
}

uint64_t wink_mcs51_wdt_last_feed_us(void) {
    // Review hardening: neutral on unbound (never crash).
    Cms8sPriv* priv = cms8s_priv(nullptr);
    return (priv != nullptr) ? priv->sys.wdt_last_feed_us : 0u;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
uint32_t wink_mcs51_wdt_overflow_total(void) {
    return s_wdt_triggered;
}

void cms8s_sys_notify_sfr_write(struct Mcu51Context* ctx, uint8_t addr) {
    if (!ctx) ctx = mcs51_get_context();
    if (!ctx) return;  // review hardening: null-active never crashes
    if (!has_wdt(ctx)) {
        return;  // classic parts have no TA window at all
    }
    if (addr == SFR_TA || addr == SFR_CLKDIV || addr == SFR_WDCON) {
        return;  // sequence keys + the consuming protected write itself
    }
    if (cms8s_priv(ctx)->sys.ta_phase != 0u) {
        // Silicon drops the half-open window when any other SFR access
        // slips between the keys (GAP-07): the pending protected write
        // then arrives locked and rolls back in its own hook.
        cms8s_priv(ctx)->sys.ta_phase = 0u;
    }
}

}  // extern "C"
