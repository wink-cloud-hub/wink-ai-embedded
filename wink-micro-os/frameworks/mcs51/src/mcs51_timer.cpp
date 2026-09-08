// SPDX-License-Identifier: Apache-2.0
// MCS-51 Timer0/Timer1 functional model (M2, AD-2 / ADR-0072).
#include "wink_mcs51_timer.h"

#include "mcs51_proxy.hpp"
#include "mcs51_context.h"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_isr.h"
#include "wink_mcs51_strict.h"

#include <cstdint>

namespace {

constexpr uint8_t  SFR_TCON = 0x88;
constexpr uint8_t  SFR_TMOD = 0x89;
constexpr uint8_t  SFR_TL0  = 0x8A;
constexpr uint8_t  SFR_TL1  = 0x8B;
constexpr uint8_t  SFR_TH0  = 0x8C;
constexpr uint8_t  SFR_TH1  = 0x8D;
constexpr uint8_t  SFR_IE   = 0xA8;

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

uint32_t reload_period(uint8_t t, uint8_t mode) {
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
        tm.running = false;
        tm.next_ovf_us = NO_OVERFLOW;
        wink_mcs51_unsupported(MCS51_FEAT_TIMER_EXT_CLK, "Timer C/T=1 (external clock)");
        return;
    }

    uint32_t period = reload_period(t, tm.mode);
    if (period == 0u) {
        tm.next_ovf_us = NO_OVERFLOW;
    } else {
        tm.next_ovf_us = from_us + period;
    }
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

    if (!tm.running) {
        tm.next_ovf_us = NO_OVERFLOW;
        return;
    }

    if (tm.mode == 2) {
        uint8_t th_addr = (t == 0) ? SFR_TH0 : SFR_TH1;
        uint8_t tl_addr = (t == 0) ? SFR_TL0 : SFR_TL1;
        mcs51_get_context()->sfr_shadow[tl_addr] = sfr(th_addr);
        uint32_t period = reload_period(t, 2);
        if (period == 0u) {
            tm.next_ovf_us = NO_OVERFLOW;
        } else {
            tm.next_ovf_us = at_us + period;
        }
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

}  // namespace

extern "C" {

void wink_mcs51_timers_step_to(uint64_t now_us) {
    step_timer(0, now_us);
    step_timer(1, now_us);
}

void mcs51_timer_reset(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    for (uint8_t t = 0; t < 2; ++t) {
        ctx->timer.channels[t] = Mcu51TimerChannel{};
        ctx->timer.channels[t].next_ovf_us = NO_OVERFLOW;
    }
}

void wink_mcs51_timer_on_read(uint8_t addr) {
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

    if (addr == SFR_TH0 || addr == SFR_TL0 ||
        addr == SFR_TH1 || addr == SFR_TL1) {
        uint8_t t = (addr == SFR_TH0 || addr == SFR_TL0) ? 0 : 1;
        if (get_tm(t).running) {
            schedule_from_reload(t, now);
        }
    }
}

static void sfr_read_hook_timer(struct Mcu51Context* ctx, uint8_t addr) {
    (void)ctx;
    wink_mcs51_timer_on_read(addr);
}

static void sfr_write_hook_timer(struct Mcu51Context* ctx, uint8_t addr,
                                 uint8_t old_val, uint8_t new_val) {
    (void)ctx;
    (void)old_val;
    (void)new_val;
    wink_mcs51_timer_on_write(addr);
}

void mcs51_timer_init(struct Mcu51Context* ctx) {
    mcs51_timer_reset(ctx);
    mcs51_trap_register_sfr_read(SFR_TCON, sfr_read_hook_timer);
    mcs51_trap_register_sfr_write(SFR_TCON, sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_TMOD, sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_TL0, sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_TL1, sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_TH0, sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_TH1, sfr_write_hook_timer);
}

void mcs51_timer_poll(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    wink_mcs51_timers_step_to(ctx->virtual_us);
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
    return earliest;
}

}  // extern "C"
