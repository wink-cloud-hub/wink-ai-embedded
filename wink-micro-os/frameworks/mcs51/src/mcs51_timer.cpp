// SPDX-License-Identifier: Apache-2.0
// MCS-51 Timer0/Timer1 functional model (M2, AD-2 / ADR-0072).
//
// See wink_mcs51_timer.h. Timers are modeled as count-up segments against the
// virtual slave clock: a segment starts at a known virtual time with a known
// counter value (latched from THx/TLx), and the next overflow instant is
// precomputed in virtual microseconds. Catch-up walks due overflows; each
// overflow sets TFx in the TCON shadow and vectors the ISR (gated by EA/ETx),
// after which the segment is re-based (mode 2: automatic reload from THx;
// mode 1/0: software reload latched from THx/TLx, the universal Keil idiom).
//
// All state is plain POD (zero-init BSS) — static-init safe (ADR-0072 D5).
#include "wink_mcs51_timer.h"

#include "mcs51_proxy.hpp"
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

constexpr uint8_t  VECTOR_T0 = 1u;
constexpr uint8_t  VECTOR_T1 = 3u;

constexpr uint64_t NO_OVERFLOW = UINT64_MAX;
// Pathological guard: never dispatch more than this many overflows in one
// catch-up call (a misconfigured sub-microsecond period cannot hang the sim).
constexpr uint32_t MAX_OVERFLOWS_PER_STEP = 1u << 20;

struct TimerModel {
    bool     running;
    bool     external_clk;  // TMOD C/T == 1: external pin count (not modeled)
    uint8_t  mode;          // 0/1/2 (mode 3 not modeled)
    uint64_t next_ovf_us;   // virtual time of next overflow
    uint8_t  tr_prev;       // last observed TRx level (edge detection)
};

TimerModel s_timers[2] = {};

uint8_t sfr(uint8_t addr) { return wink_mcs51_sfr_shadow[addr]; }
void    sfr_set_bit(uint8_t addr, uint8_t bit) {
    wink_mcs51_sfr_shadow[addr] |= static_cast<uint8_t>(1u << bit);
}
void    sfr_clear_bit(uint8_t addr, uint8_t bit) {
    wink_mcs51_sfr_shadow[addr] &= static_cast<uint8_t>(~(1u << bit));
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

// Counter maximum and reload-derived period (in counts == virtual us).
uint32_t timer_max_count(uint8_t mode) {
    if (mode == 2) return 256u;
    if (mode == 0) return 8192u;
    return 65536u;  // mode 1
}

uint32_t reload_period(uint8_t t, uint8_t mode) {
    if (mode == 2) {
        // 8-bit auto-reload: THx holds the reload value.
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

// (Re)base a counting segment at virtual time `now_us` from the THx/TLx
// shadow and schedule the next overflow.
void schedule_from_reload(uint8_t t, uint64_t now_us) {
    TimerModel& tm = s_timers[t];
    tm.mode = timer_mode(t);
    tm.external_clk = timer_external(t);
    if (tm.external_clk || tm.mode == 3) {
        // External pin counting and Timer0 mode-3 split are not modeled at
        // functional level (no time source / no ISR impact); timer stays idle.
        // Flag the unmodeled feature through the STRICT mechanism (release:
        // warn once; STRICT build: assert) — the configuration was actually
        // selected, so silence here would hide a wrong simulation result.
        if (tm.external_clk) {
            wink_mcs51_unsupported(MCS51_FEAT_TIMER_EXT_CLK,
                                   "timer external C/T pin clock");
        }
        // Mode 3 is the split-8-bit configuration on Timer 0 (unmodeled). On
        // Timer 1 mode 3 simply halts the counter, which running=false below
        // already reproduces, so no report there.
        if (tm.mode == 3 && t == 0) {
            wink_mcs51_unsupported(MCS51_FEAT_TIMER_MODE3,
                                   "Timer0 mode 3 (split 8-bit)");
        }
        tm.running = false;
        tm.next_ovf_us = NO_OVERFLOW;
        return;
    }
    tm.running = true;
    uint32_t period = reload_period(t, tm.mode);
    if (period == 0u) {
        tm.next_ovf_us = NO_OVERFLOW;
        return;
    }
    if (tm.mode == 2) {
        // First segment ends when TLx (current count) reaches 0x100.
        uint8_t tl = sfr(t == 0 ? SFR_TL0 : SFR_TL1);
        uint32_t remaining = 256u - tl;
        tm.next_ovf_us = now_us + (remaining == 0u ? period : remaining);
    } else {
        tm.next_ovf_us = now_us + period;
    }
}

void timer_start(uint8_t t, uint64_t now_us) {
    schedule_from_reload(t, now_us);
}

void timer_stop(uint8_t t) {
    s_timers[t].running = false;
    s_timers[t].next_ovf_us = NO_OVERFLOW;
}

void on_overflow(uint8_t t, uint64_t at_us) {
    TimerModel& tm = s_timers[t];
    uint8_t tcon_bit = (t == 0) ? TCON_TF0 : TCON_TF1;
    uint8_t vector   = (t == 0) ? VECTOR_T0 : VECTOR_T1;
    uint8_t et_bit   = (t == 0) ? IE_ET0 : IE_ET1;

    // Overflow latches TFx unconditionally.
    sfr_set_bit(SFR_TCON, tcon_bit);

    // Vector the ISR when the interrupt is enabled (EA + ETx). Hardware
    // clears TFx automatically when vectored; a polled/disabled TFx stays
    // for software to clear.
    uint8_t ie = sfr(SFR_IE);
    bool enabled = (ie & (1u << IE_EA)) && (ie & (1u << et_bit));
    if (enabled && wink_mcs51_dispatch_vector(vector) != 0u) {
        sfr_clear_bit(SFR_TCON, tcon_bit);
    }

    // The ISR (or a polling handler) may have stopped the timer by clearing
    // TRx; respect that instead of re-arming.
    uint8_t tr_bit = (t == 0) ? TCON_TR0 : TCON_TR1;
    if ((sfr(SFR_TCON) & static_cast<uint8_t>(1u << tr_bit)) == 0u) {
        timer_stop(t);
        return;
    }

    // Schedule the next overflow.
    if (tm.mode == 2) {
        uint8_t th_addr = (t == 0) ? SFR_TH0 : SFR_TH1;
        uint8_t tl_addr = (t == 0) ? SFR_TL0 : SFR_TL1;
        // Automatic reload: TLx := THx, visible in the shadow.
        wink_mcs51_sfr_shadow[tl_addr] = sfr(th_addr);
        uint32_t period = reload_period(t, 2);
        if (period == 0u) {
            tm.next_ovf_us = NO_OVERFLOW;
        } else {
            tm.next_ovf_us = at_us + period;
        }
    } else {
        // Mode 1/0: software (typically the ISR) reloaded THx/TLx; re-base
        // the segment at the overflow instant.
        schedule_from_reload(t, at_us);
    }
}

void step_timer(uint8_t t, uint64_t now_us) {
    TimerModel& tm = s_timers[t];
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
            break;  // ISR/handler stopped the timer (TRx cleared)
        }
    }
}

}  // namespace

extern "C" {

void wink_mcs51_timers_step_to(uint64_t now_us) {
    step_timer(0, now_us);
    step_timer(1, now_us);
}

void wink_mcs51_timers_reset(void) {
    for (uint8_t t = 0; t < 2; ++t) {
        s_timers[t] = TimerModel{};
        s_timers[t].next_ovf_us = NO_OVERFLOW;
    }
}

void wink_mcs51_timer_on_read(uint8_t addr) {
    if (addr == SFR_TCON) {
        // Lazy evaluation: a TCON/TFx poll must observe due overflows.
        wink_mcs51_timers_step_to(wink_mcs51_virtual_us());
    }
}

void wink_mcs51_timer_on_write(uint8_t addr) {
    const uint64_t now = wink_mcs51_virtual_us();

    if (addr == SFR_TCON) {
        // TR0/TR1 edges start/stop counting.
        uint8_t tcon = sfr(SFR_TCON);
        uint8_t tr[2] = {static_cast<uint8_t>((tcon >> TCON_TR0) & 1u),
                         static_cast<uint8_t>((tcon >> TCON_TR1) & 1u)};
        for (uint8_t t = 0; t < 2; ++t) {
            if (tr[t] != s_timers[t].tr_prev) {
                s_timers[t].tr_prev = tr[t];
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
        // Mode/clock-source change: re-base running timers from the shadow.
        for (uint8_t t = 0; t < 2; ++t) {
            if (s_timers[t].running) {
                schedule_from_reload(t, now);
            }
        }
        return;
    }

    if (addr == SFR_TH0 || addr == SFR_TL0 ||
        addr == SFR_TH1 || addr == SFR_TL1) {
        // Reload/count write: re-base a running timer from the new value
        // (ISR reload in mode 1, THx reload update in mode 2).
        uint8_t t = (addr == SFR_TH0 || addr == SFR_TL0) ? 0 : 1;
        if (s_timers[t].running) {
            schedule_from_reload(t, now);
        }
    }
}

}  // extern "C"
