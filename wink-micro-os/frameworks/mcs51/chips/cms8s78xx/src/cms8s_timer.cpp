// SPDX-License-Identifier: Apache-2.0
// CMS8S78xx extended-timer model (Stage4 S4-2 Step 2, PLAN-20260911-MCS51-S4,
// CPL-05).
//
// Standard 8051/8052 timers (T0/T1, T2 skeleton) stay in the generic core.
// This silicon adds Timer3/Timer4, a T2 capture/compare unit (CCEN/CCLx),
// write-0-to-clear flag registers (T2IF/EIF2) and pin-share selectors for
// external-clock/capture inputs. Moved verbatim from the generic timer TU
// and adapted to chip-pool state (S2-2 D6 handover).
//
// S4-D4 (T2 skeleton stays core): the shared T2CON/TL2/TH2/RCAP write hooks
// stay core-owned, so compare re-arm on THOSE writes arrives here through
// chained hooks (below): the chip hook runs the core C-ABI handler first,
// then re-derives the compare schedule — the exact old trigger set,
// including same-value rewrites (value-fingerprinting would miss those).
// CCEN/CCLx/CCHx writes are chip-owned hooks doing the same derivation.
// Fire-then-sync ordering preserves the old overflow/compare coincidence
// semantics.
#include "mcs51_context.h"
#include "mcs51_family.h"
#include "mcs51_trap.h"
#include "mcs51_sfr_map.h"
#include "cms8s_priv.h"
#include "cms8s_sfr_map.h"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_isr.h"
#include "wink_mcs51_timer.h"

#include <stdint.h>

extern "C" {
uint8_t js_pal_gpio_read_state(uint16_t pin);
}

namespace {

constexpr uint8_t  SFR_T2CON = MCS51_SFR_T2CON;
constexpr uint8_t  SFR_T2IF  = 0xC9;
constexpr uint8_t  SFR_RLDL  = 0xCA;
constexpr uint8_t  SFR_RLDH  = 0xCB;
constexpr uint8_t  SFR_TL2   = 0xCC;
constexpr uint8_t  SFR_TH2   = 0xCD;
constexpr uint8_t  SFR_CCEN  = 0xCE;
constexpr uint8_t  SFR_CCL1  = 0xC2;
constexpr uint8_t  SFR_CCH1  = 0xC3;
constexpr uint8_t  SFR_CCL2  = 0xC4;
constexpr uint8_t  SFR_CCH2  = 0xC5;
constexpr uint8_t  SFR_CCL3  = 0xC6;
constexpr uint8_t  SFR_CCH3  = 0xC7;
constexpr uint8_t  SFR_T2IE  = 0xCF;
constexpr uint8_t  SFR_EIE2  = CMS8S_SFR_EIE2;
constexpr uint8_t  SFR_EIF2  = CMS8S_SFR_EIF2;
constexpr uint8_t  SFR_T34MOD = CMS8S_SFR_T34MOD;
constexpr uint8_t  SFR_TL3   = 0xDA;
constexpr uint8_t  SFR_TH3   = 0xDB;
constexpr uint8_t  SFR_TL4   = 0xE2;
constexpr uint8_t  SFR_TH4   = 0xE3;

constexpr uint16_t XSFR_PS_T0   = CMS8S_XSFR_PS_T0;
constexpr uint16_t XSFR_PS_T0G  = CMS8S_XSFR_PS_T0G;
constexpr uint16_t XSFR_PS_T1   = CMS8S_XSFR_PS_T1;
constexpr uint16_t XSFR_PS_T1G  = CMS8S_XSFR_PS_T1G;
constexpr uint16_t XSFR_PS_T2   = CMS8S_XSFR_PS_T2;
constexpr uint16_t XSFR_PS_T2EX = CMS8S_XSFR_PS_T2EX;
constexpr uint16_t XSFR_PS_CAP0 = CMS8S_XSFR_PS_CAP0;
constexpr uint8_t  PS_RESET     = CMS8S_XSFR_PS_RESET;

constexpr uint8_t  T2IF_T2F    = 7u;

constexpr uint8_t  T34MOD_TR3 = 3u;
constexpr uint8_t  T34MOD_T3M = 2u;
constexpr uint8_t  T34MOD_TR4 = 7u;
constexpr uint8_t  T34MOD_T4M = 6u;

constexpr uint8_t  EIF2_TF3   = 0u;
constexpr uint8_t  EIF2_TF4   = 1u;
constexpr uint8_t  EIE2_ET3IE = 0u;
constexpr uint8_t  EIE2_ET4IE = 1u;

constexpr uint64_t NO_OVERFLOW = UINT64_MAX;
constexpr uint32_t MAX_OVERFLOWS_PER_STEP = 1u << 20;

inline uint8_t rd(Mcu51Context* ctx, uint8_t addr) {
    return ctx->sfr_shadow[addr];
}

inline void flag_set(Mcu51Context* ctx, uint8_t addr, uint8_t bit) {
    ctx->sfr_shadow[addr] =
        static_cast<uint8_t>(ctx->sfr_shadow[addr] | (1u << bit));
}

// Fsys-parameterized period (clamped to 1 us to keep the scheduler
// advancing).
uint32_t ext_counts_to_us_at(uint32_t counts, uint32_t divider) {
    const uint32_t fsys = wink_mcs51_get_clock_hz();
    const uint64_t scaled =
        static_cast<uint64_t>(counts) * static_cast<uint64_t>(divider);
    const uint64_t us = (scaled * 1000000ull) / static_cast<uint64_t>(fsys);
    return static_cast<uint32_t>(us == 0ull ? 1ull : us);
}

// T3/T4 counter clock (T34MOD): TnM=0 -> Fsys/12, TnM=1 -> Fsys/4.
uint32_t timer34_counts_to_us(Mcu51Context* ctx, uint32_t counts,
                               uint8_t select_bit) {
    const uint32_t divider =
        (rd(ctx, SFR_T34MOD) & (1u << select_bit)) ? 4u : 12u;
    return ext_counts_to_us_at(counts, divider);
}

inline uint8_t timer3_mode(Mcu51Context* ctx) {
    return rd(ctx, SFR_T34MOD) & 0x03u;
}

inline uint8_t timer4_mode(Mcu51Context* ctx) {
    return (rd(ctx, SFR_T34MOD) >> 4) & 0x03u;
}

uint32_t timer3_reload_period(Mcu51Context* ctx) {
    uint8_t mode = timer3_mode(ctx);
    uint32_t counts;
    if (mode == 2) {
        uint8_t th = rd(ctx, SFR_TH3);
        counts = static_cast<uint32_t>(256u - th);
    } else if (mode == 1) {
        uint16_t val =
            (static_cast<uint16_t>(rd(ctx, SFR_TH3)) << 8) | rd(ctx, SFR_TL3);
        counts = 65536u - val;
    } else if (mode == 0) {
        uint16_t val =
            (static_cast<uint16_t>(rd(ctx, SFR_TH3) & 0x1Fu) << 8) |
            rd(ctx, SFR_TL3);
        counts = 8192u - val;
    } else {
        counts = 256u - rd(ctx, SFR_TL3);
    }
    if (counts == 0u) {
        counts = (mode == 2 ? 256u : (mode == 1 ? 65536u : 8192u));
    }
    return timer34_counts_to_us(ctx, counts, T34MOD_T3M);
}

uint32_t timer3_current_period(Mcu51Context* ctx) {
    uint8_t mode = timer3_mode(ctx);
    uint32_t counts;
    if (mode == 2) {
        uint8_t tl = rd(ctx, SFR_TL3);
        counts = static_cast<uint32_t>(256u - tl);
    } else if (mode == 1) {
        uint16_t val =
            (static_cast<uint16_t>(rd(ctx, SFR_TH3)) << 8) | rd(ctx, SFR_TL3);
        counts = 65536u - val;
    } else if (mode == 0) {
        uint16_t val =
            (static_cast<uint16_t>(rd(ctx, SFR_TH3) & 0x1Fu) << 8) |
            rd(ctx, SFR_TL3);
        counts = 8192u - val;
    } else {
        counts = 256u - rd(ctx, SFR_TL3);
    }
    if (counts == 0u) {
        counts = (mode == 2 ? 256u : (mode == 1 ? 65536u : 8192u));
    }
    return timer34_counts_to_us(ctx, counts, T34MOD_T3M);
}

uint32_t timer4_reload_period(Mcu51Context* ctx) {
    uint8_t mode = timer4_mode(ctx);
    uint32_t counts;
    if (mode == 2) {
        uint8_t th = rd(ctx, SFR_TH4);
        counts = static_cast<uint32_t>(256u - th);
    } else if (mode == 1) {
        uint16_t val =
            (static_cast<uint16_t>(rd(ctx, SFR_TH4)) << 8) | rd(ctx, SFR_TL4);
        counts = 65536u - val;
    } else if (mode == 0) {
        uint16_t val =
            (static_cast<uint16_t>(rd(ctx, SFR_TH4) & 0x1Fu) << 8) |
            rd(ctx, SFR_TL4);
        counts = 8192u - val;
    } else {
        counts = 256u - rd(ctx, SFR_TL4);
    }
    if (counts == 0u) {
        counts = (mode == 2 ? 256u : (mode == 1 ? 65536u : 8192u));
    }
    return timer34_counts_to_us(ctx, counts, T34MOD_T4M);
}

uint32_t timer4_current_period(Mcu51Context* ctx) {
    uint8_t mode = timer4_mode(ctx);
    uint32_t counts;
    if (mode == 2) {
        uint8_t tl = rd(ctx, SFR_TL4);
        counts = static_cast<uint32_t>(256u - tl);
    } else if (mode == 1) {
        uint16_t val =
            (static_cast<uint16_t>(rd(ctx, SFR_TH4)) << 8) | rd(ctx, SFR_TL4);
        counts = 65536u - val;
    } else if (mode == 0) {
        uint16_t val =
            (static_cast<uint16_t>(rd(ctx, SFR_TH4) & 0x1Fu) << 8) |
            rd(ctx, SFR_TL4);
        counts = 8192u - val;
    } else {
        counts = 256u - rd(ctx, SFR_TL4);
    }
    if (counts == 0u) {
        counts = (mode == 2 ? 256u : (mode == 1 ? 65536u : 8192u));
    }
    return timer34_counts_to_us(ctx, counts, T34MOD_T4M);
}

void timer3_schedule_from_now(Mcu51Context* ctx, uint64_t from_us) {
    uint32_t period = timer3_current_period(ctx);
    cms8s_priv(ctx)->timer.t3_next_ovf_us = from_us + period;
}

void timer3_start(Mcu51Context* ctx, uint64_t now_us) {
    cms8s_priv(ctx)->timer.t3_running = true;
    timer3_schedule_from_now(ctx, now_us);
}

void timer3_stop(Mcu51Context* ctx) {
    cms8s_priv(ctx)->timer.t3_running = false;
    cms8s_priv(ctx)->timer.t3_next_ovf_us = NO_OVERFLOW;
}

void on_timer3_overflow(Mcu51Context* ctx, uint64_t at_us) {
    Cms8sTimerState& tm = cms8s_priv(ctx)->timer;
    flag_set(ctx, SFR_EIF2, EIF2_TF3);

    uint8_t mode = timer3_mode(ctx);
    if (mode == 2) {
        ctx->sfr_shadow[SFR_TL3] = rd(ctx, SFR_TH3);
    }

    if (!tm.t3_running) {
        tm.t3_next_ovf_us = NO_OVERFLOW;
        return;
    }

    uint32_t period = timer3_reload_period(ctx);
    tm.t3_next_ovf_us = at_us + period;

    if ((rd(ctx, SFR_EIE2) & (1u << EIE2_ET3IE)) != 0) {
        mcs51_raise_irq(IRQ_SOURCE_TIMER3);
        wink_mcs51_clear_reti_suppress();
        mcs51_irq_scan_and_dispatch();
    }
}

void step_timer3(Mcu51Context* ctx, uint64_t now_us) {
    Cms8sTimerState& tm = cms8s_priv(ctx)->timer;
    if (!tm.t3_running || tm.t3_next_ovf_us == NO_OVERFLOW) {
        return;
    }
    uint32_t fired = 0;
    while (tm.t3_next_ovf_us != NO_OVERFLOW && now_us >= tm.t3_next_ovf_us) {
        uint64_t at = tm.t3_next_ovf_us;
        on_timer3_overflow(ctx, at);
        if (++fired >= MAX_OVERFLOWS_PER_STEP) {
            timer3_stop(ctx);
            break;
        }
        if (!tm.t3_running) {
            break;
        }
    }
}

void timer4_schedule_from_now(Mcu51Context* ctx, uint64_t from_us) {
    uint32_t period = timer4_current_period(ctx);
    cms8s_priv(ctx)->timer.t4_next_ovf_us = from_us + period;
}

void timer4_start(Mcu51Context* ctx, uint64_t now_us) {
    cms8s_priv(ctx)->timer.t4_running = true;
    timer4_schedule_from_now(ctx, now_us);
}

void timer4_stop(Mcu51Context* ctx) {
    cms8s_priv(ctx)->timer.t4_running = false;
    cms8s_priv(ctx)->timer.t4_next_ovf_us = NO_OVERFLOW;
}

void on_timer4_overflow(Mcu51Context* ctx, uint64_t at_us) {
    Cms8sTimerState& tm = cms8s_priv(ctx)->timer;
    flag_set(ctx, SFR_EIF2, EIF2_TF4);

    uint8_t mode = timer4_mode(ctx);
    if (mode == 2) {
        ctx->sfr_shadow[SFR_TL4] = rd(ctx, SFR_TH4);
    }

    if (!tm.t4_running) {
        tm.t4_next_ovf_us = NO_OVERFLOW;
        return;
    }

    uint32_t period = timer4_reload_period(ctx);
    tm.t4_next_ovf_us = at_us + period;

    if ((rd(ctx, SFR_EIE2) & (1u << EIE2_ET4IE)) != 0) {
        mcs51_raise_irq(IRQ_SOURCE_TIMER4);
        wink_mcs51_clear_reti_suppress();
        mcs51_irq_scan_and_dispatch();
    }
}

void step_timer4(Mcu51Context* ctx, uint64_t now_us) {
    Cms8sTimerState& tm = cms8s_priv(ctx)->timer;
    if (!tm.t4_running || tm.t4_next_ovf_us == NO_OVERFLOW) {
        return;
    }
    uint32_t fired = 0;
    while (tm.t4_next_ovf_us != NO_OVERFLOW && now_us >= tm.t4_next_ovf_us) {
        uint64_t at = tm.t4_next_ovf_us;
        on_timer4_overflow(ctx, at);
        if (++fired >= MAX_OVERFLOWS_PER_STEP) {
            timer4_stop(ctx);
            break;
        }
        if (!tm.t4_running) {
            break;
        }
    }
}

// 16-bit SFR pair read (H:L order as laid out on silicon).
inline uint16_t rd16(Mcu51Context* ctx, uint8_t hi_addr, uint8_t lo_addr) {
    const uint16_t hi = static_cast<uint16_t>(rd(ctx, hi_addr));
    return static_cast<uint16_t>((hi << 8) | rd(ctx, lo_addr));
}

static uint16_t timer2_compare_value(Mcu51Context* ctx, uint8_t c) {
    if (c == 0) {
        return rd16(ctx, SFR_RLDH, SFR_RLDL);
    } else if (c == 1) {
        return rd16(ctx, SFR_CCH1, SFR_CCL1);
    } else if (c == 2) {
        return rd16(ctx, SFR_CCH2, SFR_CCL2);
    } else if (c == 3) {
        return rd16(ctx, SFR_CCH3, SFR_CCL3);
    }
    return 0u;
}

void clear_compares(Mcu51Context* ctx) {
    Cms8sTimerState& tm = cms8s_priv(ctx)->timer;
    for (uint8_t c = 0; c < 4; ++c) {
        tm.t2_next_cmp_us[c] = NO_OVERFLOW;
    }
}

// Re-derive compare arming (S4-D4): mirrors the old write-hook reschedule
// triggers exactly — called from the chained T2 hooks below, never polled.
// S4-D4 micro-deviation: leaving timing mode while running disarms compares
// (the old code left them stale); no test covers it, silicon-manual-plausible.
void sync_compares(Mcu51Context* ctx, uint64_t from_us) {
    Cms8sTimerState& tm = cms8s_priv(ctx)->timer;
    if (!ctx->timer.t2_running || (rd(ctx, SFR_T2CON) & 0x03u) != 1u) {
        clear_compares(ctx);
        return;
    }

    uint8_t th2 = rd(ctx, SFR_TH2);
    uint8_t tl2 = rd(ctx, SFR_TL2);
    uint16_t cur_val = (static_cast<uint16_t>(th2) << 8) | tl2;
    uint8_t ccen = rd(ctx, SFR_CCEN);
    bool div24 = (rd(ctx, SFR_T2CON) & 0x80u) != 0;

    for (uint8_t c = 0; c < 4; ++c) {
        uint8_t mode = (ccen >> (c * 2)) & 0x03u;
        // Mode 2 is compare mode (TMR2_MODE_COMPARE = 0x02)
        if (mode == 2u) {
            uint16_t cmp_val = timer2_compare_value(ctx, c);
            if (cmp_val > cur_val) {
                uint32_t diff = cmp_val - cur_val;
                uint32_t delay_us = div24 ? diff : (diff / 2u);
                if (delay_us == 0u) {
                    delay_us = 1u;
                }
                tm.t2_next_cmp_us[c] = from_us + delay_us;
            } else {
                tm.t2_next_cmp_us[c] = NO_OVERFLOW;
            }
        } else {
            tm.t2_next_cmp_us[c] = NO_OVERFLOW;
        }
    }
}

void on_timer2_compare_match(Mcu51Context* ctx, uint8_t c) {
    Cms8sTimerState& tm = cms8s_priv(ctx)->timer;
    tm.t2_next_cmp_us[c] = NO_OVERFLOW;

    flag_set(ctx, SFR_T2IF, c);

    if ((rd(ctx, SFR_T2IE) & (1u << c)) != 0) {
        mcs51_raise_irq(IRQ_SOURCE_TIMER2);
        wink_mcs51_clear_reti_suppress();
        mcs51_irq_scan_and_dispatch();
    }
}

// Fire due compare matches FIRST (old step order: compares before overflow;
// the overflow may have just reloaded the counter in the core step, in
// which case the sync below re-arms for the next period).
void fire_due_compares(Mcu51Context* ctx, uint64_t now_us) {
    Cms8sTimerState& tm = cms8s_priv(ctx)->timer;
    if (!ctx->timer.t2_running) {
        return;
    }
    for (uint8_t c = 0; c < 4; ++c) {
        if (tm.t2_next_cmp_us[c] != NO_OVERFLOW &&
            now_us >= tm.t2_next_cmp_us[c]) {
            on_timer2_compare_match(ctx, c);
            if (!ctx->timer.t2_running) {
                return;
            }
        }
    }
}

void timer2_trigger_capture(Mcu51Context* ctx, uint8_t c) {
    if (c >= 4) {
        return;
    }
    uint8_t tl2 = ctx->sfr_shadow[SFR_TL2];
    uint8_t th2 = ctx->sfr_shadow[SFR_TH2];
    if (c == 0) {
        ctx->sfr_shadow[SFR_RLDL] = tl2;
        ctx->sfr_shadow[SFR_RLDH] = th2;
    } else if (c == 1) {
        ctx->sfr_shadow[SFR_CCL1] = tl2;
        ctx->sfr_shadow[SFR_CCH1] = th2;
    } else if (c == 2) {
        ctx->sfr_shadow[SFR_CCL2] = tl2;
        ctx->sfr_shadow[SFR_CCH2] = th2;
    } else if (c == 3) {
        ctx->sfr_shadow[SFR_CCL3] = tl2;
        ctx->sfr_shadow[SFR_CCH3] = th2;
    }

    flag_set(ctx, SFR_T2IF, c);

    if ((rd(ctx, SFR_T2IE) & (1u << c)) != 0) {
        mcs51_raise_irq(IRQ_SOURCE_TIMER2);
        wink_mcs51_clear_reti_suppress();
        mcs51_irq_scan_and_dispatch();
    }
}

uint16_t resolve_cap_pin(Mcu51Context* ctx, uint8_t c) {
    // Capture-input fallback pins: P0.0, P0.1, P1.5, P1.4.
    constexpr uint16_t FALLBACK[4] = {0u, 1u, 13u, 12u};
    if (c >= 4) {
        return 0u;
    }
    if (!ctx) {
        return FALLBACK[c];
    }
    uint8_t sel = ctx->xdata_shadow[XSFR_PS_CAP0 + c];
    uint8_t port = (sel >> 4) & 0x07u;
    uint8_t bit = sel & 0x0Fu;
    // Descriptor-driven legality (S2-2).
    const uint8_t* pin_masks = mcs51_family_desc(ctx->family)->port_pin_masks;
    if (port < 4u && bit < pin_masks[port]) {
        return static_cast<uint16_t>((port << 3) | bit);
    }
    return FALLBACK[c];
}

void sample_capture(Mcu51Context* ctx) {
    Cms8sTimerState& tm = cms8s_priv(ctx)->timer;
    uint8_t ccen = rd(ctx, SFR_CCEN);
    if (!ctx->timer.t2_running || ccen == 0) {
        return;
    }
    uint8_t t2con = rd(ctx, SFR_T2CON);
    for (uint8_t c = 0; c < 4; ++c) {
        uint8_t mode = (ccen >> (c * 2)) & 0x03u;
        // Mode 1 or 3 is capture mode (0x01 | Timer2CaptureMode)
        if (mode == 1u || mode == 3u) {
            uint16_t pin = resolve_cap_pin(ctx, c);
            uint8_t level = js_pal_gpio_read_state(pin);
            if (level <= 1u) {
                uint8_t last = tm.t2_cap_last_level[c];
                tm.t2_cap_last_level[c] = level;
                if (last <= 1u && last != level) {
                    // For CC0: T2CON.I3FR (bit 6): 1 = rising, 0 = falling
                    // For CC1~3: T2CON.CAPES (bit 5): 0 = rising, 1 = falling
                    bool want_rising = (c == 0) ? ((t2con & (1u << 6)) != 0)
                                                : ((t2con & (1u << 5)) == 0);
                    if ((want_rising && last == 0u && level == 1u) ||
                        (!want_rising && last == 1u && level == 0u)) {
                        timer2_trigger_capture(ctx, c);
                    }
                }
            }
        }
    }
}

void step_chip_timers(Mcu51Context* ctx, uint64_t now_us) {
    // Fire-then-sync: due compares fire against the pre-reschedule plan
    // (overflow coincidence), then T3/T4 advance. Compare (re)arming arrives
    // exclusively through the chained hooks below — never polled.
    fire_due_compares(ctx, now_us);
    step_timer3(ctx, now_us);
    step_timer4(ctx, now_us);
}

// S4-H2 (reset-rebuilds-registration contract): shared by init and reset
// (trap_register overwrites the same slots — idempotent).
extern "C" void sfr_read_hook_cms8s_timer(struct Mcu51Context* ctx,
                                          uint8_t addr);
extern "C" void sfr_write_hook_cms8s_timer(struct Mcu51Context* ctx,
                                           uint8_t addr, uint8_t old_val,
                                           uint8_t new_val);
extern "C" void sfr_write_hook_cms8s_timer_t2(struct Mcu51Context* ctx,
                                              uint8_t addr, uint8_t old_val,
                                              uint8_t new_val);
void install_trap_hooks(void) {
    mcs51_trap_register_sfr_read(SFR_T2IF, sfr_read_hook_cms8s_timer);
    mcs51_trap_register_sfr_write(SFR_T2IF, sfr_write_hook_cms8s_timer);
    mcs51_trap_register_sfr_read(SFR_EIF2, sfr_read_hook_cms8s_timer);
    mcs51_trap_register_sfr_write(SFR_EIF2, sfr_write_hook_cms8s_timer);
    mcs51_trap_register_sfr_read(SFR_T34MOD, sfr_read_hook_cms8s_timer);
    mcs51_trap_register_sfr_write(SFR_T34MOD, sfr_write_hook_cms8s_timer);
    mcs51_trap_register_sfr_write(SFR_TL3, sfr_write_hook_cms8s_timer);
    mcs51_trap_register_sfr_write(SFR_TH3, sfr_write_hook_cms8s_timer);
    mcs51_trap_register_sfr_write(SFR_TL4, sfr_write_hook_cms8s_timer);
    mcs51_trap_register_sfr_write(SFR_TH4, sfr_write_hook_cms8s_timer);
    // Chained T2 hooks (S4-D4): displace the core slots registered by the
    // core init earlier in the reset loop; each chains the core handler.
    mcs51_trap_register_sfr_write(SFR_T2CON, sfr_write_hook_cms8s_timer_t2);
    mcs51_trap_register_sfr_write(SFR_TL2, sfr_write_hook_cms8s_timer_t2);
    mcs51_trap_register_sfr_write(SFR_TH2, sfr_write_hook_cms8s_timer_t2);
    mcs51_trap_register_sfr_write(SFR_RLDL, sfr_write_hook_cms8s_timer_t2);
    mcs51_trap_register_sfr_write(SFR_RLDH, sfr_write_hook_cms8s_timer_t2);
    mcs51_trap_register_sfr_write(SFR_CCEN, sfr_write_hook_cms8s_timer_t2);
    mcs51_trap_register_sfr_write(SFR_CCL1, sfr_write_hook_cms8s_timer_t2);
    mcs51_trap_register_sfr_write(SFR_CCH1, sfr_write_hook_cms8s_timer_t2);
    mcs51_trap_register_sfr_write(SFR_CCL2, sfr_write_hook_cms8s_timer_t2);
    mcs51_trap_register_sfr_write(SFR_CCH2, sfr_write_hook_cms8s_timer_t2);
    mcs51_trap_register_sfr_write(SFR_CCL3, sfr_write_hook_cms8s_timer_t2);
    mcs51_trap_register_sfr_write(SFR_CCH3, sfr_write_hook_cms8s_timer_t2);
}

// M3: C language linkage for the C-ABI hook table.
extern "C" void sfr_write_hook_cms8s_timer(struct Mcu51Context* ctx,
                                           uint8_t addr, uint8_t old_val,
                                           uint8_t new_val) {
    if (addr == SFR_T2IF || addr == SFR_EIF2) {
        // Interrupt flag registers are write-0-to-clear (W0C): writing 0
        // clears that bit; writing 1 leaves that bit unchanged (GAP-12).
        if (ctx) {
            ctx->sfr_shadow[addr] = old_val & new_val;
        }
        return;
    }
    if (!ctx) {
        ctx = mcs51_get_context();
    }
    if (!ctx) {
        return;
    }
    const uint64_t now = wink_mcs51_virtual_us();
    if (addr == SFR_T34MOD) {
        uint8_t t34mod = rd(ctx, SFR_T34MOD);
        uint8_t tr3 = (t34mod >> T34MOD_TR3) & 1u;
        uint8_t tr4 = (t34mod >> T34MOD_TR4) & 1u;
        Cms8sTimerState& tm = cms8s_priv(ctx)->timer;

        if (tr3 != tm.t3_tr_prev) {
            tm.t3_tr_prev = tr3;
            if (tr3) {
                timer3_start(ctx, now);
            } else {
                timer3_stop(ctx);
            }
        } else if (tm.t3_running) {
            timer3_schedule_from_now(ctx, now);
        }

        if (tr4 != tm.t4_tr_prev) {
            tm.t4_tr_prev = tr4;
            if (tr4) {
                timer4_start(ctx, now);
            } else {
                timer4_stop(ctx);
            }
        } else if (tm.t4_running) {
            timer4_schedule_from_now(ctx, now);
        }
        return;
    }

    if (addr == SFR_TL3 || addr == SFR_TH3) {
        Cms8sTimerState& tm = cms8s_priv(ctx)->timer;
        if (tm.t3_running) {
            timer3_schedule_from_now(ctx, now);
        }
        return;
    }

    if (addr == SFR_TL4 || addr == SFR_TH4) {
        Cms8sTimerState& tm = cms8s_priv(ctx)->timer;
        if (tm.t4_running) {
            timer4_schedule_from_now(ctx, now);
        }
        return;
    }
}

// Chained hooks for core-owned T2 addresses (S4-D4): the core handler runs
// first (overflow re-arm / start-stop edge), then the compare schedule is
// re-derived — the exact old trigger set. Registered AFTER the core init in
// the reset loop, so these displace the core slots on chip families.
extern "C" void sfr_write_hook_cms8s_timer_t2(struct Mcu51Context* ctx,
                                   uint8_t addr, uint8_t old_val,
                                   uint8_t new_val) {
    (void)old_val;
    (void)new_val;
    if (!ctx) {
        ctx = mcs51_get_context();
    }
    if (!ctx) {
        return;
    }
    wink_mcs51_timer_on_write(addr);  // core: overflow re-arm / start-stop
    sync_compares(ctx, wink_mcs51_virtual_us());
}

extern "C" void sfr_read_hook_cms8s_timer(struct Mcu51Context* ctx,
                                          uint8_t addr) {
    (void)addr;
    if (!ctx) {
        ctx = mcs51_get_context();
    }
    if (!cms8s_hook_armed(ctx)) {
        return;
    }
    fire_due_compares(ctx, ctx->virtual_us);
    step_timer3(ctx, ctx->virtual_us);
    step_timer4(ctx, ctx->virtual_us);
}

}  // namespace

extern "C" {

// GAP-12 white-box test surface (moved from the core TU with the code).
uint32_t wink_mcs51_test_timer3_reload_period(void) {
    Mcu51Context* ctx = mcs51_get_context();
    return (ctx != nullptr) ? timer3_reload_period(ctx) : 0u;
}

uint32_t wink_mcs51_test_timer4_reload_period(void) {
    Mcu51Context* ctx = mcs51_get_context();
    return (ctx != nullptr) ? timer4_reload_period(ctx) : 0u;
}

// Forward: init delegates to reset below (S4-H2 follow-up).
void cms8s_timer_reset(struct Mcu51Context* ctx);

void cms8s_timer_init(struct Mcu51Context* ctx) {
    // S4-H2 follow-up: init delegates to reset so a standalone init seeds the
    // pin-share selectors and installs hooks on its own; the full context
    // reset runs both passes (idempotent overwrite).
    cms8s_timer_reset(ctx);
}

void cms8s_timer_reset(struct Mcu51Context* ctx) {
    if (!ctx) {
        ctx = mcs51_get_context();
    }
    if (!ctx) {
        return;
    }
    if (ctx->family != MCS51_FAMILY_CMS8S78XX) {
        return;
    }
    cms8s_soc_bind(ctx);  // defensive: standalone resets bind too
    // Re-patch (context memset cleared the slots) + re-seed selectors.
    ctx->timer.t0_ps_addr = XSFR_PS_T0;
    ctx->timer.t1_ps_addr = XSFR_PS_T1;
    ctx->timer.t2_ps_addr = XSFR_PS_T2;
    ctx->xdata_shadow[XSFR_PS_T0] = PS_RESET;
    ctx->xdata_shadow[XSFR_PS_T0G] = PS_RESET;
    ctx->xdata_shadow[XSFR_PS_T1] = PS_RESET;
    ctx->xdata_shadow[XSFR_PS_T1G] = PS_RESET;
    ctx->xdata_shadow[XSFR_PS_T2] = PS_RESET;
    ctx->xdata_shadow[XSFR_PS_T2EX] = PS_RESET;
    ctx->xdata_shadow[XSFR_PS_CAP0] = PS_RESET;
    ctx->xdata_shadow[XSFR_PS_CAP0 + 1] = PS_RESET;
    ctx->xdata_shadow[XSFR_PS_CAP0 + 2] = PS_RESET;
    ctx->xdata_shadow[XSFR_PS_CAP0 + 3] = PS_RESET;
    Cms8sTimerState& tm = cms8s_priv(ctx)->timer;
    tm.t3_running = false;
    tm.t3_next_ovf_us = NO_OVERFLOW;
    tm.t3_tr_prev = 0;
    tm.t4_running = false;
    tm.t4_next_ovf_us = NO_OVERFLOW;
    tm.t4_tr_prev = 0;
    for (uint8_t c = 0; c < 4; ++c) {
        tm.t2_cap_last_level[c] = 0xFFu;
        tm.t2_next_cmp_us[c] = NO_OVERFLOW;
    }
    // NOTE: hook registration lives in install_trap_hooks (called by init);
    // reset re-arms state AND reinstalls the slots (S4-H2).
    install_trap_hooks();
}

void cms8s_timer_poll(struct Mcu51Context* ctx) {
    if (!ctx) {
        ctx = mcs51_get_context();
    }
    if (!cms8s_hook_armed(ctx)) {
        return;  // review hardening: unbound/classic
    }
    step_chip_timers(ctx, ctx->virtual_us);
    sample_capture(ctx);
}

// Test/dispatch stepping without capture sampling (mirrors the core
// wink_mcs51_timers_step_to split: event advancement only).
void cms8s_timer_step_to(uint64_t now_us) {
    Mcu51Context* ctx = mcs51_get_context();
    if (!cms8s_hook_armed(ctx)) {
        return;
    }
    if (!ctx) {
        return;
    }
    // NOTE: step targets the ACTIVE context (test seam, like the core
    // step_to). Multi-context callers set the active context first.
    step_chip_timers(ctx, now_us);
}

uint64_t cms8s_timer_next_event_us(struct Mcu51Context* ctx) {
    if (!ctx) {
        ctx = mcs51_get_context();
    }
    if (!cms8s_hook_armed(ctx)) {
        return UINT64_MAX;
    }
    Cms8sTimerState& tm = cms8s_priv(ctx)->timer;
    uint64_t earliest = UINT64_MAX;
    if (ctx->timer.t2_running) {
        for (uint8_t c = 0; c < 4; ++c) {
            if (tm.t2_next_cmp_us[c] != NO_OVERFLOW &&
                tm.t2_next_cmp_us[c] < earliest) {
                earliest = tm.t2_next_cmp_us[c];
            }
        }
    }
    if (tm.t3_running && tm.t3_next_ovf_us != NO_OVERFLOW) {
        if (tm.t3_next_ovf_us < earliest) {
            earliest = tm.t3_next_ovf_us;
        }
    }
    if (tm.t4_running && tm.t4_next_ovf_us != NO_OVERFLOW) {
        if (tm.t4_next_ovf_us < earliest) {
            earliest = tm.t4_next_ovf_us;
        }
    }
    return earliest;
}

}  // extern "C"
