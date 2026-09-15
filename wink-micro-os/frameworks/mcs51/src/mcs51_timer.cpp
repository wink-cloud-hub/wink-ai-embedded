// SPDX-License-Identifier: GPL-3.0-only
// MCS-51 Timer0/Timer1 + standard Timer2 skeleton (M2, AD-2 / ADR-0072).
#include "wink_mcs51_timer.h"

#include "mcs51_proxy.hpp"
#include "mcs51_context.h"
#include "mcs51_family.h"
#include "mcs51_sfr_map.h"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_isr.h"
#include "wink_mcs51_strict.h"
#include <cstdint>
#include "pal_log.h"

extern "C" uint8_t js_pal_gpio_read_state(uint16_t pin);

namespace {

constexpr uint8_t  SFR_TCON = 0x88;
constexpr uint8_t  SFR_TMOD = 0x89;
// M5: shared addresses alias the single generic source;
// timer-private standard addresses keep literals below. Stage4 CPL-05:
// T3/T4, capture/compare and flag registers live in the chip package —
// this TU keeps only numeric standard-skeleton addresses (T2CON/TL2/TH2/
// RCAP pair, T2 flag/enable bytes) plus the CKCON divider address.
constexpr uint8_t  SFR_CKCON = MCS51_SFR_CKCON;
constexpr uint8_t  CKCON_T0M = 3u;
constexpr uint8_t  CKCON_T1M = 4u;
constexpr uint8_t  SFR_TL0  = 0x8A;
constexpr uint8_t  SFR_TL1  = 0x8B;
constexpr uint8_t  SFR_TH0  = 0x8C;
constexpr uint8_t  SFR_TH1  = 0x8D;
constexpr uint8_t  SFR_T2CON = MCS51_SFR_T2CON;
constexpr uint8_t  SFR_T2IF  = 0xC9;  // S4-D4: overflow flag byte, chip-owned
constexpr uint8_t  SFR_RLDL  = 0xCA;  // RCAP2L / reload low (shared semantics)
constexpr uint8_t  SFR_RLDH  = 0xCB;  // RCAP2H / reload high (shared semantics)
constexpr uint8_t  SFR_TL2   = 0xCC;
constexpr uint8_t  SFR_TH2   = 0xCD;
constexpr uint8_t  SFR_T2IE  = 0xCF;  // S4-D4: T2 interrupt-enable byte

constexpr uint8_t  T2IF_T2F    = 7u;
constexpr uint8_t  T2IE_T2OVIE = 7u;

// No pin-share selector: the ext-clk input is bond-fixed to its fallback
// pin. Chip inits patch real selector addresses (extint precedent).
constexpr uint16_t SELECTOR_NONE = 0xFFFFu;

constexpr uint8_t  TCON_TR0 = 4u;
constexpr uint8_t  TCON_TF0 = 5u;
constexpr uint8_t  TCON_TR1 = 6u;
constexpr uint8_t  TCON_TF1 = 7u;

constexpr uint8_t  IE_ET0 = 1u;
constexpr uint8_t  IE_ET1 = 3u;
constexpr uint8_t  IE_EA  = 7u;

constexpr uint64_t NO_OVERFLOW = UINT64_MAX;
constexpr uint32_t MAX_OVERFLOWS_PER_STEP = 1u << 20;

inline uint8_t sfr(uint8_t addr) {
    return mcs51_get_context()->sfr_shadow[addr];
}
inline void sfr_set_bit(uint8_t addr, uint8_t bit) {
    mcs51_get_context()->sfr_shadow[addr] |= static_cast<uint8_t>(1u << bit);
}
inline void sfr_clear_bit(uint8_t addr, uint8_t bit) {
    mcs51_get_context()->sfr_shadow[addr] &= static_cast<uint8_t>(~(1u << bit));
}

inline Mcu51TimerChannel& get_tm(uint8_t t) {
    return mcs51_get_context()->timer.channels[t];
}

uint8_t timer_mode(uint8_t t) {
    uint8_t tmod = sfr(SFR_TMOD);
    uint8_t shift = t == 0 ? 0 : 4;
    return static_cast<uint8_t>((tmod >> shift) & 0x3u);
}

bool timer_external(uint8_t t) {
    uint8_t tmod = sfr(SFR_TMOD);
    uint8_t bit = t == 0 ? 2 : 6;
    return (tmod & static_cast<uint8_t>(1u << bit)) != 0;
}

uint32_t timer_max_count(uint8_t mode) {
    if (mode == 2) return 256u;
    if (mode == 0) return 8192u;
    return 65536u;  // mode 1
}

uint32_t reload_counts(uint8_t t, uint8_t mode) {
    if (mode == 2) {
        uint8_t th = sfr(t == 0 ? SFR_TH0 : SFR_TH1);
        return static_cast<uint32_t>(256u - th);
    }
    uint8_t th = sfr(t == 0 ? SFR_TH0 : SFR_TH1);
    uint8_t tl = sfr(t == 0 ? SFR_TL0 : SFR_TL1);
    uint32_t base;
    if (mode == 0) {
        base = static_cast<uint32_t>((th & 0x1Fu) << 8) | tl;  // 13-bit
    } else {
        base = static_cast<uint32_t>(th) << 8 | tl;            // 16-bit
    }
    uint32_t period = timer_max_count(mode) - base;
    return period == 0u ? timer_max_count(mode) : period;
}

// Clock-control divider register (TnM = 0 -> Fsys/12, TnM = 1 -> Fsys/4):
// us = counts * divider * 1e6 / Fsys. With the 12 MHz framework default and
// 12T this yields the historical 1 count = 1 us mapping.
uint64_t counts_to_us(uint8_t t, uint32_t counts) {
    const uint8_t ckcon = sfr(SFR_CKCON);
    const uint8_t bit = t == 0 ? CKCON_T0M : CKCON_T1M;
    const uint32_t divider = (ckcon & (1u << bit)) ? 4u : 12u;
    const uint32_t fsys = wink_mcs51_get_clock_hz();
    uint64_t us = (static_cast<uint64_t>(counts) * static_cast<uint64_t>(divider) *
                   1000000ull) / static_cast<uint64_t>(fsys);
    return us == 0ull ? 1ull : us;
}

// GAP-12: Fsys-parameterized period for T2/T3/T4 (the old code hardcoded
// the 24 MHz reciprocals counts/2 and counts/6). divider is the counter
// clock's Fsys divisor; clamp to 1 us to keep the scheduler advancing.
uint32_t ext_counts_to_us(uint32_t counts, uint32_t divider) {
    const uint32_t fsys = wink_mcs51_get_clock_hz();
    uint64_t us = (static_cast<uint64_t>(counts) * static_cast<uint64_t>(divider) *
                   1000000ull) / static_cast<uint64_t>(fsys);
    return static_cast<uint32_t>(us == 0ull ? 1ull : us);
}

// T2 counter clock: T2PS=0 -> Fsys/12, T2PS=1 -> Fsys/24 (S4-D4: prescale
// bit retained numerically in the T2 skeleton).
uint32_t timer2_counts_to_us(uint32_t counts) {
    const uint32_t divider = (sfr(SFR_T2CON) & 0x80u) ? 24u : 12u;
    return ext_counts_to_us(counts, divider);
}

uint64_t reload_period_us(uint8_t t, uint8_t mode) {
    return counts_to_us(t, reload_counts(t, mode));
}

void schedule_from_reload(uint8_t t, uint64_t from_us) {
    Mcu51TimerChannel& tm = get_tm(t);
    tm.mode = timer_mode(t);
    tm.external_clk = timer_external(t);

    if (tm.mode == 3) {
        tm.running = false;
        tm.next_ovf_us = NO_OVERFLOW;
        wink_mcs51_unsupported(MCS51_FEAT_TIMER_MODE3, "Timer Mode 3");
        return;
    }
    if (tm.external_clk) {
        tm.running = true;
        tm.next_ovf_us = NO_OVERFLOW;
        return;
    }

    tm.next_ovf_us = from_us + reload_period_us(t, tm.mode);
}

void timer_start(uint8_t t, uint64_t now_us) {
    Mcu51TimerChannel& tm = get_tm(t);
    tm.running = true;
    schedule_from_reload(t, now_us);
}

void timer_stop(uint8_t t) {
    Mcu51TimerChannel& tm = get_tm(t);
    tm.running = false;
    tm.next_ovf_us = NO_OVERFLOW;
}

void on_overflow(uint8_t t, uint64_t at_us) {
    Mcu51TimerChannel& tm = get_tm(t);
    uint8_t tf_bit = (t == 0) ? TCON_TF0 : TCON_TF1;
    sfr_set_bit(SFR_TCON, tf_bit);

    mcs51_raise_irq(t == 0 ? IRQ_SOURCE_TIMER0 : IRQ_SOURCE_TIMER1);

    wink_mcs51_clear_reti_suppress();
    mcs51_irq_scan_and_dispatch();

    if (!tm.running || tm.external_clk) {
        tm.next_ovf_us = NO_OVERFLOW;
        return;
    }

    if (tm.mode == 2) {
        uint8_t th_addr = (t == 0) ? SFR_TH0 : SFR_TH1;
        uint8_t tl_addr = (t == 0) ? SFR_TL0 : SFR_TL1;
        mcs51_get_context()->sfr_shadow[tl_addr] = sfr(th_addr);
        tm.next_ovf_us = at_us + reload_period_us(t, 2);
    } else {
        schedule_from_reload(t, at_us);
    }
}

void step_timer(uint8_t t, uint64_t now_us) {
    Mcu51TimerChannel& tm = get_tm(t);
    if (!tm.running || tm.next_ovf_us == NO_OVERFLOW) {
        return;
    }
    uint32_t fired = 0;
    while (tm.next_ovf_us != NO_OVERFLOW && now_us >= tm.next_ovf_us) {
        uint64_t at = tm.next_ovf_us;
        on_overflow(t, at);
        if (++fired >= MAX_OVERFLOWS_PER_STEP) {
            timer_stop(t);
            break;
        }
        if (!tm.running) {
            break;
        }
    }
}

uint32_t timer2_reload_period(void) {
    uint8_t rldh = sfr(SFR_RLDH);
    uint8_t rldl = sfr(SFR_RLDL);
    uint16_t reload_val = (static_cast<uint16_t>(rldh) << 8) | rldl;
    uint32_t counts = 65536u - reload_val;
    if (counts == 0u) counts = 65536u;
    uint8_t t2con = sfr(SFR_T2CON);
    if ((t2con & 0x80u) != 0) {
        return counts;
    }
    return timer2_counts_to_us(counts);
}

uint32_t timer2_current_period(void) {
    uint8_t th2 = sfr(SFR_TH2);
    uint8_t tl2 = sfr(SFR_TL2);
    uint16_t cur_val = (static_cast<uint16_t>(th2) << 8) | tl2;
    uint32_t counts = 65536u - cur_val;
    if (counts == 0u) counts = 65536u;
    uint8_t t2con = sfr(SFR_T2CON);
    if ((t2con & 0x80u) != 0) {
        return counts;
    }
    return timer2_counts_to_us(counts);
}

inline uint8_t timer2_mode(void) {
    return sfr(SFR_T2CON) & 0x03u;
}

void timer2_schedule_from_now(uint64_t from_us) {
    // Stage4 CPL-05: compare scheduling moved to the chip package (driven
    // by its shadow fingerprint); the core schedules the overflow only.
    Mcu51TimerState& tm = mcs51_get_context()->timer;
    uint32_t period = timer2_current_period();
    tm.t2_next_ovf_us = from_us + period;
}

void timer2_start(uint64_t now_us) {
    Mcu51TimerState& tm = mcs51_get_context()->timer;
    tm.t2_running = true;
    if (timer2_mode() == 1u) {
        timer2_schedule_from_now(now_us);
    } else {
        tm.t2_next_ovf_us = NO_OVERFLOW;
    }
}

void timer2_stop(void) {
    Mcu51TimerState& tm = mcs51_get_context()->timer;
    tm.t2_running = false;
    tm.t2_next_ovf_us = NO_OVERFLOW;
    // Compare disarm follows in the chip sync (running bit is fingerprinted).
}

void on_timer2_overflow(uint64_t at_us) {
    Mcu51TimerState& tm = mcs51_get_context()->timer;
    sfr_set_bit(SFR_T2IF, T2IF_T2F);

    uint8_t t2con = sfr(SFR_T2CON);
    if ((t2con & 0x30u) != 0) { // T2Rn != 0: auto-reload or T2EX reload
        uint8_t rldl = sfr(SFR_RLDL);
        uint8_t rldh = sfr(SFR_RLDH);
        mcs51_get_context()->sfr_shadow[SFR_TL2] = rldl;
        mcs51_get_context()->sfr_shadow[SFR_TH2] = rldh;
    } else {
        mcs51_get_context()->sfr_shadow[SFR_TL2] = 0;
        mcs51_get_context()->sfr_shadow[SFR_TH2] = 0;
    }

    if (!tm.t2_running) {
        tm.t2_next_ovf_us = NO_OVERFLOW;
        return;
    }

    if (timer2_mode() == 1u) {
        if ((t2con & 0x30u) != 0) {
            uint32_t period = timer2_reload_period();
            tm.t2_next_ovf_us = at_us + period;
        } else {
            // LOAD_DISABLE: wait for software reload in ISR (e.g. TMR2_ConfigTimerPeriod)
            tm.t2_next_ovf_us = NO_OVERFLOW;
        }
    } else {
        tm.t2_next_ovf_us = NO_OVERFLOW;
    }

    if ((sfr(SFR_T2IE) & (1u << T2IE_T2OVIE)) != 0) {
        mcs51_raise_irq(IRQ_SOURCE_TIMER2);
        wink_mcs51_clear_reti_suppress();
        mcs51_irq_scan_and_dispatch();
    }
}

void step_timer2(uint64_t now_us) {
    // Stage4 CPL-05: compare-match expiry moved to the chip package.
    Mcu51TimerState& tm = mcs51_get_context()->timer;
    if (!tm.t2_running) {
        return;
    }

    // Process overflow events
    if (tm.t2_next_ovf_us == NO_OVERFLOW) {
        return;
    }
    uint32_t fired = 0;
    while (tm.t2_next_ovf_us != NO_OVERFLOW && now_us >= tm.t2_next_ovf_us) {
        uint64_t at = tm.t2_next_ovf_us;
        on_timer2_overflow(at);
        if (++fired >= MAX_OVERFLOWS_PER_STEP) {
            timer2_stop();
            break;
        }
        if (!tm.t2_running) {
            break;
        }
    }
}

}  // namespace

extern "C" {

// S4-H2: hook implementations live below; reset registers them, so forward
// declarations precede it (same static-in-extern-"C" pattern as the
// definitions — proven on all three toolchains).
static void sfr_read_hook_timer(struct Mcu51Context* ctx, uint8_t addr);
static void sfr_write_hook_timer(struct Mcu51Context* ctx, uint8_t addr,
                                 uint8_t old_val, uint8_t new_val);

// GAP-12 white-box test surface: Fsys-parameterized reload period for T2
// (the calculator is file-local in the anonymous namespace; T3/T4 twins
// live in the chip package under the same C-ABI names).
uint32_t wink_mcs51_test_timer2_reload_period(void) { return timer2_reload_period(); }

void wink_mcs51_timers_step_to(uint64_t now_us) {
    // Stage4 CPL-05: T3/T4 + T2-compare stepping moved to the chip package
    // (chip poll/step entry points); the core advances T0/T1/standard-T2.
    step_timer(0, now_us);
    step_timer(1, now_us);
    step_timer2(now_us);
}

void wink_mcs51_timer_pulse(uint8_t t) {
    if (t == 2) {
        Mcu51TimerState& tm = mcs51_get_context()->timer;
        if (!tm.t2_running || timer2_mode() != 2u) return;
        uint16_t val = (static_cast<uint16_t>(sfr(SFR_TH2)) << 8) | sfr(SFR_TL2);
        val++;
        if (val == 0u) {
            uint8_t rldh = sfr(SFR_RLDH);
            uint8_t rldl = sfr(SFR_RLDL);
            mcs51_get_context()->sfr_shadow[SFR_TH2] = rldh;
            mcs51_get_context()->sfr_shadow[SFR_TL2] = rldl;
            on_timer2_overflow(wink_mcs51_virtual_us());
        } else {
            mcs51_get_context()->sfr_shadow[SFR_TH2] = static_cast<uint8_t>((val >> 8) & 0xFFu);
            mcs51_get_context()->sfr_shadow[SFR_TL2] = static_cast<uint8_t>(val & 0xFFu);
        }
        return;
    }
    if (t > 2) return;
    Mcu51TimerChannel& tm = get_tm(t);
    if (!tm.running || !tm.external_clk) return;

    if (tm.mode == 1) {
        // 16-bit counter mode
        uint8_t th_addr = (t == 0) ? SFR_TH0 : SFR_TH1;
        uint8_t tl_addr = (t == 0) ? SFR_TL0 : SFR_TL1;
        uint16_t val = (static_cast<uint16_t>(sfr(th_addr)) << 8) | sfr(tl_addr);
        val++;
        mcs51_get_context()->sfr_shadow[th_addr] = static_cast<uint8_t>((val >> 8) & 0xFFu);
        mcs51_get_context()->sfr_shadow[tl_addr] = static_cast<uint8_t>(val & 0xFFu);
        if (val == 0u) {
            on_overflow(t, wink_mcs51_virtual_us());
        }
    } else if (tm.mode == 2) {
        // 8-bit auto-reload counter mode
        uint8_t th_addr = (t == 0) ? SFR_TH0 : SFR_TH1;
        uint8_t tl_addr = (t == 0) ? SFR_TL0 : SFR_TL1;
        uint8_t val = sfr(tl_addr);
        val++;
        if (val == 0u) {
            mcs51_get_context()->sfr_shadow[tl_addr] = sfr(th_addr);
            on_overflow(t, wink_mcs51_virtual_us());
        } else {
            mcs51_get_context()->sfr_shadow[tl_addr] = val;
        }
    } else if (tm.mode == 0) {
        // 13-bit counter mode
        uint8_t th_addr = (t == 0) ? SFR_TH0 : SFR_TH1;
        uint8_t tl_addr = (t == 0) ? SFR_TL0 : SFR_TL1;
        uint16_t val = (static_cast<uint16_t>(sfr(th_addr) & 0x1Fu) << 8) | sfr(tl_addr);
        val = (val + 1) & 0x1FFFu;
        mcs51_get_context()->sfr_shadow[th_addr] = static_cast<uint8_t>((sfr(th_addr) & 0xE0u) | ((val >> 8) & 0x1Fu));
        mcs51_get_context()->sfr_shadow[tl_addr] = static_cast<uint8_t>(val & 0xFFu);
        if (val == 0u) {
            on_overflow(t, wink_mcs51_virtual_us());
        }
    }
}

void mcs51_timer_reset(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    for (uint8_t t = 0; t < 2; ++t) {
        ctx->timer.channels[t] = Mcu51TimerChannel{};
        ctx->timer.channels[t].next_ovf_us = NO_OVERFLOW;
        ctx->timer.channels[t].last_pin_level = 0xFFu;
    }
    ctx->timer.t2_running = false;
    ctx->timer.t2_next_ovf_us = NO_OVERFLOW;
    ctx->timer.t2_tr_prev = 0;
    ctx->timer.t2_last_pin_level = 0xFFu;
    // Generic ext-clk selector slots: none = hardwired fallback pins. The
    // owning chip init patches its selector addresses (extint precedent);
    // chip-owned PS seeds live in the chip reset (stage4 CPL-05).
    ctx->timer.t0_ps_addr = SELECTOR_NONE;
    ctx->timer.t1_ps_addr = SELECTOR_NONE;
    ctx->timer.t2_ps_addr = SELECTOR_NONE;
    // S4-H2: registration lives here (init delegates above).
    mcs51_trap_register_sfr_read(SFR_TCON, sfr_read_hook_timer);
    mcs51_trap_register_sfr_write(SFR_TCON, sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_TMOD, sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_TL0, sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_TL1, sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_TH0, sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_TH1, sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_T2CON, sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_TL2, sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_TH2, sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_RLDL, sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_RLDH, sfr_write_hook_timer);
}

void wink_mcs51_timer_on_read(uint8_t addr) {
    // Stage4 CPL-05: flag/control-block reads (T2IF/EIF2/T34MOD) step the
    // chip timers through the chip read hooks; the core steps T0/T1/T2 here.
    if (addr == SFR_TCON) {
        wink_mcs51_timers_step_to(wink_mcs51_virtual_us());
    }
}

void wink_mcs51_timer_on_write(uint8_t addr) {
    const uint64_t now = wink_mcs51_virtual_us();

    if (addr == SFR_TCON) {
        uint8_t tcon = sfr(SFR_TCON);
        uint8_t tr[2] = {static_cast<uint8_t>((tcon >> TCON_TR0) & 1u),
                         static_cast<uint8_t>((tcon >> TCON_TR1) & 1u)};
        for (uint8_t t = 0; t < 2; ++t) {
            if (tr[t] != get_tm(t).tr_prev) {
                get_tm(t).tr_prev = tr[t];
                if (tr[t]) {
                    timer_start(t, now);
                } else {
                    timer_stop(t);
                }
            }
        }
        return;
    }

    if (addr == SFR_TMOD) {
        for (uint8_t t = 0; t < 2; ++t) {
            if (get_tm(t).running) {
                schedule_from_reload(t, now);
            }
        }
        return;
    }

    if (addr == SFR_CKCON) {
        // T0M/T1M (or WTS WDT coeff) changed: reschedule live timers
        for (uint8_t t = 0; t < 2; ++t) {
            if (get_tm(t).running) {
                schedule_from_reload(t, now);
            }
        }
        return;
    }

    if (addr == SFR_TH0 || addr == SFR_TL0 ||
        addr == SFR_TH1 || addr == SFR_TL1) {
        uint8_t t = (addr == SFR_TH0 || addr == SFR_TL0) ? 0 : 1;
        if (get_tm(t).running) {
            schedule_from_reload(t, now);
        }
        return;
    }

    if (addr == SFR_T2CON) {
        uint8_t t2con = sfr(SFR_T2CON);
        uint8_t running = (t2con & 0x03u) != 0 ? 1u : 0u;
        Mcu51TimerState& tm = mcs51_get_context()->timer;
        if (running != tm.t2_tr_prev) {
            tm.t2_tr_prev = running;
            if (running) {
                timer2_start(now);
            } else {
                timer2_stop();
            }
        }
        return;
    }

    if (addr == SFR_TL2 || addr == SFR_TH2 ||
        addr == SFR_RLDL || addr == SFR_RLDH) {
        // Stage4 CPL-05: compare re-arm on these writes is derived by the
        // chip fingerprint; the core reschedules the overflow only.
        Mcu51TimerState& tm = mcs51_get_context()->timer;
        if (tm.t2_running && timer2_mode() == 1u) {
            timer2_schedule_from_now(now);
        }
        return;
    }
}

static void sfr_read_hook_timer(struct Mcu51Context* ctx, uint8_t addr) {
    (void)ctx;
    wink_mcs51_timer_on_read(addr);
}

static void sfr_write_hook_timer(struct Mcu51Context* ctx, uint8_t addr,
                                 uint8_t old_val, uint8_t new_val) {
    // Stage4 CPL-05: flag W0C interception moved to the chip package.
    (void)ctx;
    (void)old_val;
    (void)new_val;
    wink_mcs51_timer_on_write(addr);
}

void mcs51_timer_init(struct Mcu51Context* ctx) {
    // S4-H2 (reset-rebuilds-registration contract): hook registration lives
    // in reset below — init delegates, so both paths install the slots.
    // (T2IF/EIF2/T34MOD/TL3/TH3/TL4/TH4/CCEN/CCLx/CCHx slots belong to the
    // chip package, stage4 CPL-05.)
    mcs51_timer_reset(ctx);
}

static uint16_t resolve_timer_pin(struct Mcu51Context* ctx, uint8_t t) {
    // Model defaults retained (P3.4/P3.5); the selector slot is patched by
    // the owning chip init, none (= hardwired) on classic parts.
    uint16_t fallback = (t == 0) ? 28u : 29u;
    if (!ctx) {
        return fallback;
    }
    uint16_t ps_addr = (t == 0) ? ctx->timer.t0_ps_addr : ctx->timer.t1_ps_addr;
    if (ps_addr == SELECTOR_NONE) {
        return fallback;
    }
    uint8_t sel = ctx->xdata_shadow[ps_addr];
    uint8_t port = (sel >> 4) & 0x07u;
    uint8_t bit = sel & 0x0Fu;
    // S2-2: legality from the family descriptor — the full-pin family opens
    // P3.4/P3.5 ext-clk (pins 28/29, previously mis-rejected); the reduced
    // family keeps its masks. No hardcoded table (was PORT_PINS).
    if (mcs51_family_pin_valid(mcs51_family_desc(ctx->family), port, bit)) {
        return static_cast<uint16_t>((port << 3) | bit);
    }
    return fallback;
}

static uint16_t resolve_timer2_pin(struct Mcu51Context* ctx) {
    uint16_t fallback = 14u;  // model default T2 external input
    if (!ctx) {
        return fallback;
    }
    if (ctx->timer.t2_ps_addr == SELECTOR_NONE) {
        return fallback;
    }
    uint8_t sel = ctx->xdata_shadow[ctx->timer.t2_ps_addr];
    uint8_t port = (sel >> 4) & 0x07u;
    uint8_t bit = sel & 0x0Fu;
    // S2-2: descriptor-driven legality (see resolve_timer_pin).
    if (mcs51_family_pin_valid(mcs51_family_desc(ctx->family), port, bit)) {
        return static_cast<uint16_t>((port << 3) | bit);
    }
    return fallback;
}

void mcs51_timer_poll(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    wink_mcs51_timers_step_to(ctx->virtual_us);

    // Poll external clock pins T0 (default pin 28) and T1 (default pin 29),
    // or the chip-selected pin when a selector is patched.
    for (uint8_t t = 0; t < 2; ++t) {
        Mcu51TimerChannel& tm = ctx->timer.channels[t];
        if (tm.running && tm.external_clk) {
            uint16_t pin = resolve_timer_pin(ctx, t);
            uint8_t level = js_pal_gpio_read_state(pin);
            if (level <= 1u) {
                uint8_t last = tm.last_pin_level;
                tm.last_pin_level = level;
                if (last == 1u && level == 0u) {
                    wink_mcs51_timer_pulse(t);
                }
            }
        }
    }

    // Poll external clock pin T2 (default pin 14) or the chip-selected pin.
    if (ctx->timer.t2_running && timer2_mode() == 2u) {
        uint16_t pin = resolve_timer2_pin(ctx);
        uint8_t level = js_pal_gpio_read_state(pin);
        if (level <= 1u) {
            uint8_t last = ctx->timer.t2_last_pin_level;
            ctx->timer.t2_last_pin_level = level;
            if (last == 1u && level == 0u) {
                wink_mcs51_timer_pulse(2);
            }
        }
    }

    // Stage4 CPL-05: T2 capture/compare sampling moved to the chip package.
}

uint64_t mcs51_timer_next_event_us(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    uint64_t earliest = UINT64_MAX;
    for (uint8_t t = 0; t < 2; ++t) {
        const Mcu51TimerChannel& tm = ctx->timer.channels[t];
        if (tm.running && tm.next_ovf_us != NO_OVERFLOW) {
            if (tm.next_ovf_us < earliest) {
                earliest = tm.next_ovf_us;
            }
        }
    }
    if (ctx->timer.t2_running && ctx->timer.t2_next_ovf_us != NO_OVERFLOW) {
        if (ctx->timer.t2_next_ovf_us < earliest) {
            earliest = ctx->timer.t2_next_ovf_us;
        }
    }
    // Stage4 CPL-05: compare + T3/T4 next-events advertised by the chip.
    return earliest;
}

}  // extern "C"
