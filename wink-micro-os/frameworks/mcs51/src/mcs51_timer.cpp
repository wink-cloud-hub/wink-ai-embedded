// SPDX-License-Identifier: Apache-2.0
// MCS-51 Timer0/Timer1 functional model (M2, AD-2 / ADR-0072).
#include "wink_mcs51_timer.h"

#include "mcs51_proxy.hpp"
#include "mcs51_context.h"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_isr.h"
#include "wink_mcs51_strict.h"
#include <cstdint>
#include "pal_log.h"

extern "C" uint8_t js_pal_gpio_read_state(uint16_t pin);

namespace {

constexpr uint8_t  SFR_TCON = 0x88;
constexpr uint8_t  SFR_TMOD = 0x89;
constexpr uint8_t  SFR_CKCON = 0x8E;
constexpr uint8_t  CKCON_T0M = 3u;
constexpr uint8_t  CKCON_T1M = 4u;
constexpr uint8_t  SFR_TL0  = 0x8A;
constexpr uint8_t  SFR_TL1  = 0x8B;
constexpr uint8_t  SFR_TH0  = 0x8C;
constexpr uint8_t  SFR_TH1  = 0x8D;
constexpr uint8_t  SFR_CCL1  = 0xC2;
constexpr uint8_t  SFR_CCH1  = 0xC3;
constexpr uint8_t  SFR_CCL2  = 0xC4;
constexpr uint8_t  SFR_CCH2  = 0xC5;
constexpr uint8_t  SFR_CCL3  = 0xC6;
constexpr uint8_t  SFR_CCH3  = 0xC7;
constexpr uint8_t  SFR_T2CON = 0xC8;
constexpr uint8_t  SFR_T2IF  = 0xC9;
constexpr uint8_t  SFR_RLDL  = 0xCA;
constexpr uint8_t  SFR_RLDH  = 0xCB;
constexpr uint8_t  SFR_TL2   = 0xCC;
constexpr uint8_t  SFR_TH2   = 0xCD;
constexpr uint8_t  SFR_CCEN  = 0xCE;
constexpr uint8_t  SFR_T2IE  = 0xCF;
constexpr uint8_t  SFR_EIE2  = 0xAA;
constexpr uint8_t  SFR_EIF2  = 0xB2;
constexpr uint8_t  SFR_T34MOD = 0xD2;
constexpr uint8_t  SFR_TL3   = 0xDA;
constexpr uint8_t  SFR_TH3   = 0xDB;
constexpr uint8_t  SFR_TL4   = 0xE2;
constexpr uint8_t  SFR_TH4   = 0xE3;

constexpr uint8_t  T2IF_T2F    = 7u;
constexpr uint8_t  T2IE_T2OVIE = 7u;

constexpr uint8_t  T34MOD_TR3 = 3u;
constexpr uint8_t  T34MOD_T3M = 2u;
constexpr uint8_t  T34MOD_TR4 = 7u;
constexpr uint8_t  T34MOD_T4M = 6u;

constexpr uint8_t  EIF2_TF3   = 0u;
constexpr uint8_t  EIF2_TF4   = 1u;
constexpr uint8_t  EIE2_ET3IE = 0u;
constexpr uint8_t  EIE2_ET4IE = 1u;

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

// CMS8S78xx Timer0/1 input clock (ref manual 9.2.7 CKCON.T0M/T1M):
//   TnM = 0 -> Fsys/12 (classic 12T), TnM = 1 -> Fsys/4 (1T/4).
// us = counts * divider * 1e6 / Fsys. With the 12 MHz framework default and
// 12T this yields the historical 1 count = 1 us mapping (classic STC/AT89).
uint64_t counts_to_us(uint8_t t, uint32_t counts) {
    const uint8_t ckcon = sfr(SFR_CKCON);
    const uint8_t bit = t == 0 ? CKCON_T0M : CKCON_T1M;
    const uint32_t divider = (ckcon & (1u << bit)) ? 4u : 12u;
    const uint32_t fsys = wink_mcs51_get_clock_hz();
    uint64_t us = (static_cast<uint64_t>(counts) * static_cast<uint64_t>(divider) *
                   1000000ull) / static_cast<uint64_t>(fsys);
    return us == 0ull ? 1ull : us;
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
    uint32_t us = counts / 2u;
    return us == 0u ? 1u : us;
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
    uint32_t us = counts / 2u;
    return us == 0u ? 1u : us;
}

inline uint8_t timer2_mode(void) {
    return sfr(SFR_T2CON) & 0x03u;
}

static uint16_t timer2_compare_value(uint8_t c) {
    if (c == 0) {
        return (static_cast<uint16_t>(sfr(SFR_RLDH)) << 8) | sfr(SFR_RLDL);
    } else if (c == 1) {
        return (static_cast<uint16_t>(sfr(SFR_CCH1)) << 8) | sfr(SFR_CCL1);
    } else if (c == 2) {
        return (static_cast<uint16_t>(sfr(SFR_CCH2)) << 8) | sfr(SFR_CCL2);
    } else if (c == 3) {
        return (static_cast<uint16_t>(sfr(SFR_CCH3)) << 8) | sfr(SFR_CCL3);
    }
    return 0u;
}

void timer2_schedule_from_now(uint64_t from_us) {
    Mcu51TimerState& tm = mcs51_get_context()->timer;
    uint32_t period = timer2_current_period();
    tm.t2_next_ovf_us = from_us + period;

    uint8_t th2 = sfr(SFR_TH2);
    uint8_t tl2 = sfr(SFR_TL2);
    uint16_t cur_val = (static_cast<uint16_t>(th2) << 8) | tl2;
    uint8_t ccen = sfr(SFR_CCEN);
    uint8_t t2con = sfr(SFR_T2CON);
    bool div24 = (t2con & 0x80u) != 0;

    for (uint8_t c = 0; c < 4; ++c) {
        uint8_t mode = (ccen >> (c * 2)) & 0x03u;
        // Mode 2 is compare mode (TMR2_MODE_COMPARE = 0x02)
        if (mode == 2u) {
            uint16_t cmp_val = timer2_compare_value(c);
            if (cmp_val > cur_val) {
                uint32_t diff = cmp_val - cur_val;
                uint32_t delay_us = div24 ? diff : (diff / 2u);
                if (delay_us == 0u) delay_us = 1u;
                tm.t2_next_cmp_us[c] = from_us + delay_us;
            } else {
                tm.t2_next_cmp_us[c] = NO_OVERFLOW;
            }
        } else {
            tm.t2_next_cmp_us[c] = NO_OVERFLOW;
        }
    }
}

void timer2_start(uint64_t now_us) {
    Mcu51TimerState& tm = mcs51_get_context()->timer;
    tm.t2_running = true;
    if (timer2_mode() == 1u) {
        timer2_schedule_from_now(now_us);
    } else {
        tm.t2_next_ovf_us = NO_OVERFLOW;
        for (uint8_t c = 0; c < 4; ++c) {
            tm.t2_next_cmp_us[c] = NO_OVERFLOW;
        }
    }
}

void timer2_stop(void) {
    Mcu51TimerState& tm = mcs51_get_context()->timer;
    tm.t2_running = false;
    tm.t2_next_ovf_us = NO_OVERFLOW;
    for (uint8_t c = 0; c < 4; ++c) {
        tm.t2_next_cmp_us[c] = NO_OVERFLOW;
    }
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

static void on_timer2_compare_match(uint8_t c) {
    Mcu51Context* ctx = mcs51_get_context();
    Mcu51TimerState& tm = ctx->timer;
    tm.t2_next_cmp_us[c] = NO_OVERFLOW;

    sfr_set_bit(SFR_T2IF, c);

    if ((sfr(SFR_T2IE) & (1u << c)) != 0) {
        mcs51_raise_irq(IRQ_SOURCE_TIMER2);
        wink_mcs51_clear_reti_suppress();
        mcs51_irq_scan_and_dispatch();
    }
}

void step_timer2(uint64_t now_us) {
    Mcu51TimerState& tm = mcs51_get_context()->timer;
    if (!tm.t2_running) {
        return;
    }

    // Process compare match events first
    for (uint8_t c = 0; c < 4; ++c) {
        if (tm.t2_next_cmp_us[c] != NO_OVERFLOW && now_us >= tm.t2_next_cmp_us[c]) {
            on_timer2_compare_match(c);
            if (!tm.t2_running) {
                return;
            }
        }
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

inline uint8_t timer3_mode(void) {
    return sfr(SFR_T34MOD) & 0x03u;
}

inline uint8_t timer4_mode(void) {
    return (sfr(SFR_T34MOD) >> 4) & 0x03u;
}

uint32_t timer3_reload_period(void) {
    uint8_t mode = timer3_mode();
    uint32_t counts;
    if (mode == 2) {
        uint8_t th = sfr(SFR_TH3);
        counts = static_cast<uint32_t>(256u - th);
    } else if (mode == 1) {
        uint16_t val = (static_cast<uint16_t>(sfr(SFR_TH3)) << 8) | sfr(SFR_TL3);
        counts = 65536u - val;
    } else if (mode == 0) {
        uint16_t val = (static_cast<uint16_t>(sfr(SFR_TH3) & 0x1Fu) << 8) | sfr(SFR_TL3);
        counts = 8192u - val;
    } else {
        counts = 256u - sfr(SFR_TL3);
    }
    if (counts == 0u) counts = (mode == 2 ? 256u : (mode == 1 ? 65536u : 8192u));
    uint8_t t3m = (sfr(SFR_T34MOD) >> T34MOD_T3M) & 0x01u;
    if (t3m == 0) {
        uint32_t us = counts / 2u;
        return us == 0u ? 1u : us;
    } else {
        uint32_t us = counts / 6u;
        return us == 0u ? 1u : us;
    }
}

uint32_t timer3_current_period(void) {
    uint8_t mode = timer3_mode();
    uint32_t counts;
    if (mode == 2) {
        uint8_t tl = sfr(SFR_TL3);
        counts = static_cast<uint32_t>(256u - tl);
    } else if (mode == 1) {
        uint16_t val = (static_cast<uint16_t>(sfr(SFR_TH3)) << 8) | sfr(SFR_TL3);
        counts = 65536u - val;
    } else if (mode == 0) {
        uint16_t val = (static_cast<uint16_t>(sfr(SFR_TH3) & 0x1Fu) << 8) | sfr(SFR_TL3);
        counts = 8192u - val;
    } else {
        counts = 256u - sfr(SFR_TL3);
    }
    if (counts == 0u) counts = (mode == 2 ? 256u : (mode == 1 ? 65536u : 8192u));
    uint8_t t3m = (sfr(SFR_T34MOD) >> T34MOD_T3M) & 0x01u;
    if (t3m == 0) {
        uint32_t us = counts / 2u;
        return us == 0u ? 1u : us;
    } else {
        uint32_t us = counts / 6u;
        return us == 0u ? 1u : us;
    }
}

void timer3_schedule_from_now(uint64_t from_us) {
    Mcu51TimerState& tm = mcs51_get_context()->timer;
    uint32_t period = timer3_current_period();
    tm.t3_next_ovf_us = from_us + period;
}

void timer3_start(uint64_t now_us) {
    Mcu51TimerState& tm = mcs51_get_context()->timer;
    tm.t3_running = true;
    timer3_schedule_from_now(now_us);
}

void timer3_stop(void) {
    Mcu51TimerState& tm = mcs51_get_context()->timer;
    tm.t3_running = false;
    tm.t3_next_ovf_us = NO_OVERFLOW;
}

void on_timer3_overflow(uint64_t at_us) {
    Mcu51TimerState& tm = mcs51_get_context()->timer;
    sfr_set_bit(SFR_EIF2, EIF2_TF3);

    uint8_t mode = timer3_mode();
    if (mode == 2) {
        mcs51_get_context()->sfr_shadow[SFR_TL3] = sfr(SFR_TH3);
    }

    if (!tm.t3_running) {
        tm.t3_next_ovf_us = NO_OVERFLOW;
        return;
    }

    uint32_t period = timer3_reload_period();
    tm.t3_next_ovf_us = at_us + period;

    if ((sfr(SFR_EIE2) & (1u << EIE2_ET3IE)) != 0) {
        mcs51_raise_irq(IRQ_SOURCE_TIMER3);
        wink_mcs51_clear_reti_suppress();
        mcs51_irq_scan_and_dispatch();
    }
}

void step_timer3(uint64_t now_us) {
    Mcu51TimerState& tm = mcs51_get_context()->timer;
    if (!tm.t3_running || tm.t3_next_ovf_us == NO_OVERFLOW) {
        return;
    }
    uint32_t fired = 0;
    while (tm.t3_next_ovf_us != NO_OVERFLOW && now_us >= tm.t3_next_ovf_us) {
        uint64_t at = tm.t3_next_ovf_us;
        on_timer3_overflow(at);
        if (++fired >= MAX_OVERFLOWS_PER_STEP) {
            timer3_stop();
            break;
        }
        if (!tm.t3_running) {
            break;
        }
    }
}

uint32_t timer4_reload_period(void) {
    uint8_t mode = timer4_mode();
    uint32_t counts;
    if (mode == 2) {
        uint8_t th = sfr(SFR_TH4);
        counts = static_cast<uint32_t>(256u - th);
    } else if (mode == 1) {
        uint16_t val = (static_cast<uint16_t>(sfr(SFR_TH4)) << 8) | sfr(SFR_TL4);
        counts = 65536u - val;
    } else if (mode == 0) {
        uint16_t val = (static_cast<uint16_t>(sfr(SFR_TH4) & 0x1Fu) << 8) | sfr(SFR_TL4);
        counts = 8192u - val;
    } else {
        counts = 256u - sfr(SFR_TL4);
    }
    if (counts == 0u) counts = (mode == 2 ? 256u : (mode == 1 ? 65536u : 8192u));
    uint8_t t4m = (sfr(SFR_T34MOD) >> T34MOD_T4M) & 0x01u;
    if (t4m == 0) {
        uint32_t us = counts / 2u;
        return us == 0u ? 1u : us;
    } else {
        uint32_t us = counts / 6u;
        return us == 0u ? 1u : us;
    }
}

uint32_t timer4_current_period(void) {
    uint8_t mode = timer4_mode();
    uint32_t counts;
    if (mode == 2) {
        uint8_t tl = sfr(SFR_TL4);
        counts = static_cast<uint32_t>(256u - tl);
    } else if (mode == 1) {
        uint16_t val = (static_cast<uint16_t>(sfr(SFR_TH4)) << 8) | sfr(SFR_TL4);
        counts = 65536u - val;
    } else if (mode == 0) {
        uint16_t val = (static_cast<uint16_t>(sfr(SFR_TH4) & 0x1Fu) << 8) | sfr(SFR_TL4);
        counts = 8192u - val;
    } else {
        counts = 256u - sfr(SFR_TL4);
    }
    if (counts == 0u) counts = (mode == 2 ? 256u : (mode == 1 ? 65536u : 8192u));
    uint8_t t4m = (sfr(SFR_T34MOD) >> T34MOD_T4M) & 0x01u;
    if (t4m == 0) {
        uint32_t us = counts / 2u;
        return us == 0u ? 1u : us;
    } else {
        uint32_t us = counts / 6u;
        return us == 0u ? 1u : us;
    }
}

void timer4_schedule_from_now(uint64_t from_us) {
    Mcu51TimerState& tm = mcs51_get_context()->timer;
    uint32_t period = timer4_current_period();
    tm.t4_next_ovf_us = from_us + period;
}

void timer4_start(uint64_t now_us) {
    Mcu51TimerState& tm = mcs51_get_context()->timer;
    tm.t4_running = true;
    timer4_schedule_from_now(now_us);
}

void timer4_stop(void) {
    Mcu51TimerState& tm = mcs51_get_context()->timer;
    tm.t4_running = false;
    tm.t4_next_ovf_us = NO_OVERFLOW;
}

void on_timer4_overflow(uint64_t at_us) {
    Mcu51TimerState& tm = mcs51_get_context()->timer;
    sfr_set_bit(SFR_EIF2, EIF2_TF4);

    uint8_t mode = timer4_mode();
    if (mode == 2) {
        mcs51_get_context()->sfr_shadow[SFR_TL4] = sfr(SFR_TH4);
    }

    if (!tm.t4_running) {
        tm.t4_next_ovf_us = NO_OVERFLOW;
        return;
    }

    uint32_t period = timer4_reload_period();
    tm.t4_next_ovf_us = at_us + period;

    if ((sfr(SFR_EIE2) & (1u << EIE2_ET4IE)) != 0) {
        mcs51_raise_irq(IRQ_SOURCE_TIMER4);
        wink_mcs51_clear_reti_suppress();
        mcs51_irq_scan_and_dispatch();
    }
}

void step_timer4(uint64_t now_us) {
    Mcu51TimerState& tm = mcs51_get_context()->timer;
    if (!tm.t4_running || tm.t4_next_ovf_us == NO_OVERFLOW) {
        return;
    }
    uint32_t fired = 0;
    while (tm.t4_next_ovf_us != NO_OVERFLOW && now_us >= tm.t4_next_ovf_us) {
        uint64_t at = tm.t4_next_ovf_us;
        on_timer4_overflow(at);
        if (++fired >= MAX_OVERFLOWS_PER_STEP) {
            timer4_stop();
            break;
        }
        if (!tm.t4_running) {
            break;
        }
    }
}

}  // namespace

extern "C" {

void wink_mcs51_timers_step_to(uint64_t now_us) {
    step_timer(0, now_us);
    step_timer(1, now_us);
    step_timer2(now_us);
    step_timer3(now_us);
    step_timer4(now_us);
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
    for (uint8_t c = 0; c < 4; ++c) {
        ctx->timer.t2_cap_last_level[c] = 0xFFu;
        ctx->timer.t2_next_cmp_us[c] = NO_OVERFLOW;
    }

    ctx->timer.t3_running = false;
    ctx->timer.t3_next_ovf_us = NO_OVERFLOW;
    ctx->timer.t3_tr_prev = 0;

    ctx->timer.t4_running = false;
    ctx->timer.t4_next_ovf_us = NO_OVERFLOW;
    ctx->timer.t4_tr_prev = 0;
}

void wink_mcs51_timer_on_read(uint8_t addr) {
    if (addr == SFR_TCON || addr == SFR_T2IF || addr == SFR_EIF2 || addr == SFR_T34MOD) {
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

    if (addr == SFR_TL2 || addr == SFR_TH2 || addr == SFR_RLDL || addr == SFR_RLDH ||
        addr == SFR_CCEN || (addr >= SFR_CCL1 && addr <= SFR_CCH3)) {
        Mcu51TimerState& tm = mcs51_get_context()->timer;
        if (tm.t2_running && timer2_mode() == 1u) {
            timer2_schedule_from_now(now);
        }
        return;
    }

    if (addr == SFR_T34MOD) {
        uint8_t t34mod = sfr(SFR_T34MOD);
        uint8_t tr3 = (t34mod >> T34MOD_TR3) & 1u;
        uint8_t tr4 = (t34mod >> T34MOD_TR4) & 1u;
        Mcu51TimerState& tm = mcs51_get_context()->timer;

        if (tr3 != tm.t3_tr_prev) {
            tm.t3_tr_prev = tr3;
            if (tr3) {
                timer3_start(now);
            } else {
                timer3_stop();
            }
        } else if (tm.t3_running) {
            timer3_schedule_from_now(now);
        }

        if (tr4 != tm.t4_tr_prev) {
            tm.t4_tr_prev = tr4;
            if (tr4) {
                timer4_start(now);
            } else {
                timer4_stop();
            }
        } else if (tm.t4_running) {
            timer4_schedule_from_now(now);
        }
        return;
    }

    if (addr == SFR_TL3 || addr == SFR_TH3) {
        Mcu51TimerState& tm = mcs51_get_context()->timer;
        if (tm.t3_running) {
            timer3_schedule_from_now(now);
        }
        return;
    }

    if (addr == SFR_TL4 || addr == SFR_TH4) {
        Mcu51TimerState& tm = mcs51_get_context()->timer;
        if (tm.t4_running) {
            timer4_schedule_from_now(now);
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
    if (addr == SFR_T2IF || addr == SFR_EIF2) {
        // CMS8S78xx interrupt flag registers are write-0-to-clear (W0C):
        // writing 0 clears that bit; writing 1 leaves that bit unchanged.
        if (ctx) {
            ctx->sfr_shadow[addr] = old_val & new_val;
        }
        return;
    }
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
    mcs51_trap_register_sfr_read(SFR_T2IF, sfr_read_hook_timer);
    mcs51_trap_register_sfr_write(SFR_T2IF, sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_T2CON, sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_TL2, sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_TH2, sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_RLDL, sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_RLDH, sfr_write_hook_timer);
    mcs51_trap_register_sfr_read(SFR_EIF2, sfr_read_hook_timer);
    mcs51_trap_register_sfr_write(SFR_EIF2, sfr_write_hook_timer);
    mcs51_trap_register_sfr_read(SFR_T34MOD, sfr_read_hook_timer);
    mcs51_trap_register_sfr_write(SFR_T34MOD, sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_TL3, sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_TH3, sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_TL4, sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_TH4, sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_CCEN, sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_CCL1, sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_CCH1, sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_CCL2, sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_CCH2, sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_CCL3, sfr_write_hook_timer);
    mcs51_trap_register_sfr_write(SFR_CCH3, sfr_write_hook_timer);
}

static uint16_t resolve_timer_pin(struct Mcu51Context* ctx, uint8_t t) {
    uint16_t fallback = (t == 0) ? 28u : 29u;
    uint16_t ps_addr = (t == 0) ? 0xF0C2u : 0xF0C4u;
    uint8_t sel = ctx->xdata_shadow[ps_addr];
    uint8_t port = (sel >> 4) & 0x07u;
    uint8_t bit = sel & 0x0Fu;
    constexpr uint8_t PORT_PINS[4] = {8u, 8u, 6u, 4u};
    if (port < 4u && bit < PORT_PINS[port]) {
        return static_cast<uint16_t>((port << 3) | bit);
    }
    return fallback;
}

static uint16_t resolve_timer2_pin(struct Mcu51Context* ctx) {
    uint16_t fallback = 14u; // P1.6 (CMS8S78xx default T2 external input)
    if (!ctx) return fallback;
    uint8_t sel = ctx->xdata_shadow[0xF0C6u]; // PS_T2
    uint8_t port = (sel >> 4) & 0x07u;
    uint8_t bit = sel & 0x0Fu;
    constexpr uint8_t PORT_PINS[4] = {8u, 8u, 6u, 4u};
    if (port < 4u && bit < PORT_PINS[port]) {
        return static_cast<uint16_t>((port << 3) | bit);
    }
    return fallback;
}

static uint16_t resolve_cap_pin(struct Mcu51Context* ctx, uint8_t c) {
    constexpr uint16_t FALLBACK[4] = {0u, 1u, 13u, 12u}; // P0.0, P0.1, P1.5, P1.4
    if (c >= 4) return 0u;
    if (!ctx) return FALLBACK[c];
    uint8_t sel = ctx->xdata_shadow[0xF0C8u + c];
    uint8_t port = (sel >> 4) & 0x07u;
    uint8_t bit = sel & 0x0Fu;
    constexpr uint8_t PORT_PINS[4] = {8u, 8u, 6u, 4u};
    if (port < 4u && bit < PORT_PINS[port]) {
        return static_cast<uint16_t>((port << 3) | bit);
    }
    return FALLBACK[c];
}

static void timer2_trigger_capture(struct Mcu51Context* ctx, uint8_t c) {
    if (c >= 4) return;
    uint8_t tl2 = sfr(SFR_TL2);
    uint8_t th2 = sfr(SFR_TH2);
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

    sfr_set_bit(SFR_T2IF, c);

    if ((sfr(SFR_T2IE) & (1u << c)) != 0) {
        mcs51_raise_irq(IRQ_SOURCE_TIMER2);
        wink_mcs51_clear_reti_suppress();
        mcs51_irq_scan_and_dispatch();
    }
}

void mcs51_timer_poll(struct Mcu51Context* ctx) {
    if (!ctx) ctx = mcs51_get_context();
    wink_mcs51_timers_step_to(ctx->virtual_us);

    // Poll external clock pins T0 (default P3.4 = pin 28) and T1 (default P3.5 = pin 29),
    // or port-selected pin via PS_T0 / PS_T1 (CMS8S78xx).
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

    // Poll external clock pin T2 (default P1.6 = pin 14) via PS_T2 (0xF0C6)
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

    // Poll Timer 2 Capture channels (CAP0~CAP3)
    uint8_t ccen = ctx->sfr_shadow[SFR_CCEN];
    if (ctx->timer.t2_running && ccen != 0) {
        uint8_t t2con = ctx->sfr_shadow[SFR_T2CON];
        for (uint8_t c = 0; c < 4; ++c) {
            uint8_t mode = (ccen >> (c * 2)) & 0x03u;
            // Mode 1 or 3 is capture mode (0x01 | Timer2CaptureMode)
            if (mode == 1u || mode == 3u) {
                uint16_t pin = resolve_cap_pin(ctx, c);
                uint8_t level = js_pal_gpio_read_state(pin);
                if (level <= 1u) {
                    uint8_t last = ctx->timer.t2_cap_last_level[c];
                    ctx->timer.t2_cap_last_level[c] = level;
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
    for (uint8_t c = 0; c < 4; ++c) {
        if (ctx->timer.t2_running && ctx->timer.t2_next_cmp_us[c] != NO_OVERFLOW) {
            if (ctx->timer.t2_next_cmp_us[c] < earliest) {
                earliest = ctx->timer.t2_next_cmp_us[c];
            }
        }
    }
    if (ctx->timer.t3_running && ctx->timer.t3_next_ovf_us != NO_OVERFLOW) {
        if (ctx->timer.t3_next_ovf_us < earliest) {
            earliest = ctx->timer.t3_next_ovf_us;
        }
    }
    if (ctx->timer.t4_running && ctx->timer.t4_next_ovf_us != NO_OVERFLOW) {
        if (ctx->timer.t4_next_ovf_us < earliest) {
            earliest = ctx->timer.t4_next_ovf_us;
        }
    }
    return earliest;
}

}  // extern "C"
