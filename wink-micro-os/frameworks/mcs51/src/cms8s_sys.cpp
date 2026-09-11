// SPDX-License-Identifier: Apache-2.0
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
#include "wink_mcs51_strict.h"
#include "wink_mcs51_timer.h"
#include "wink_mcs51_wdt.h"
#include "cms8s_priv.h"

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
    if (!has_wdt(ctx) || !wdt_enabled(ctx)) {
        return;
    }
    if (cms8s_priv(ctx)->sys.wdt_overflow_latched != 0u) {
        return;  // one count per arming episode until the next feed
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
        cms8s_priv(ctx)->sys.wdt_overflow_latched = 1u;
        wdt_overflow_policy();
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
    if (!consume_unlock(ctx)) {
        ctx->sfr_shadow[addr] = old_val;
        return;
    }
    // WDT reset timing is a coarse poll model (see wdt_check_impl); accepted
    // (unlocked) writes persist in the shadow register and arm/feed here.
    const bool was_re = (old_val & WDCON_WDTRE) != 0u;
    const bool now_re = (new_val & WDCON_WDTRE) != 0u;
    if (!was_re && now_re) {
        cms8s_priv(ctx)->sys.wdt_last_feed_us = ctx->virtual_us;
        cms8s_priv(ctx)->sys.wdt_overflow_latched = 0u;
    }
    if ((new_val & WDCON_WDTCLR) != 0u) {
        cms8s_priv(ctx)->sys.wdt_last_feed_us = ctx->virtual_us;
        cms8s_priv(ctx)->sys.wdt_overflow_latched = 0u;
    }
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

void cms8s_sys_reset(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    if (!ctx) return;
    cms8s_soc_bind(ctx);  // defensive: standalone resets bind too (no-op if bound)
    reset_state(ctx);
    install_dispatch(ctx);
}

void cms8s_sys_init(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    if (!ctx) return;
    cms8s_soc_bind(ctx);  // bind BEFORE any pool deref (ordering invariant)
    reset_state(ctx);
    install_dispatch(ctx);
}

void cms8s_sys_poll(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    if (!cms8s_hook_armed(ctx)) return;  // review hardening: unbound/classic
    wdt_check_impl(ctx);
}

uint64_t cms8s_sys_next_event_us(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    if (!has_wdt(ctx) || !wdt_enabled(ctx)) {
        return UINT64_MAX;
    }
    if (cms8s_priv(ctx)->sys.wdt_overflow_latched != 0u) {
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
    if (!has_wdt(ctx) || !wdt_enabled(ctx)) {
        return UINT64_MAX;
    }
    if (cms8s_priv(ctx)->sys.wdt_overflow_latched != 0u) {
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
