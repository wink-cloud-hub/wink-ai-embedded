// SPDX-License-Identifier: Apache-2.0
// MCS-51 virtual slave clock + cooperative quota engine (ADR-0072).
//
// See wink_mcs51_clock.h for the dual-clock-domain contract. Implementation
// notes:
//   * The slave clock (s_virtual_us) advances inside the fiber at interception
//     points. Each consumed quota slice is billed 1:1 to the platform master
//     clock via pal_os_busy_wait_us() (host: s_time_us += us; wasm: the JS
//     bridge advances pal_wasm virtual clock — same call Arduino's
//     delayMicroseconds uses), so physical time and 51 time stay in lockstep.
//   * The fiber itself never blocks on wall time: a duration-0 timed yield
//     (pal_os_sleep_ms(0), the same primitive Arduino's yield() uses) parks
//     the task and switches to the master, which immediately re-readies it.
//   * Catch-up (timer stepping + overflow ISR dispatch) runs on the fiber
//     side at every resume point, so timer overflows are always evaluated in
//     fiber context and ISRs can safely touch SFRs.
#include "wink_mcs51_clock.h"

#include "pal_osal.h"
#include "wink_sim_scheduler.h"

#include <cstdint>

// Defined in mcs51_isr.cpp. True while a virtual ISR is running on the fiber
// (ISRs charge time but must never yield — Trap red line 2, ADR-0072 D4).
extern "C" bool wink_mcs51_in_isr(void);

namespace {

// Slave clock: simulated 8051 microseconds since framework reset.
uint64_t s_virtual_us = 0;
// Virtual-us point at which the current fiber slice began.
uint64_t s_slice_start = 0;
// Observability counters.
uint32_t s_quota_yields = 0;
// Catch-up hook (timer models); null until registered.
wink_mcs51_catchup_fn_t s_catchup = nullptr;

bool in_fiber(void) {
    // Static initializers run before the scheduler exists; SFR access there
    // (e.g. a static-init safety test) must not touch the scheduler.
    return sim_scheduler_current_id() != SIM_SCHED_NO_READY;
}

void do_catchup(void) {
    if (s_catchup != nullptr) {
        s_catchup(s_virtual_us);
    }
}

// Duration-0 cooperative yield to the master (suppress the deprecated-use
// diagnostic: this is the sanctioned cooperative-yield bridge, same as
// frameworks/arduino/src/Common.cpp yield()).
void cooperative_yield(void) {
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
    pal_os_sleep_ms(0u);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif
}

// Bill `us` of virtual time to the master clock 1:1 (AD-14). Host advances
// s_time_us directly; wasm routes through the JS virtual-clock bridge.
void bill_master(uint32_t us) {
    pal_os_busy_wait_us(us);
}

}  // namespace

extern "C" {

void wink_mcs51_clock_reset(void) {
    s_virtual_us = 0;
    s_slice_start = 0;
    s_quota_yields = 0;
}

void wink_mcs51_set_catchup_hook(wink_mcs51_catchup_fn_t hook) {
    s_catchup = hook;
}

uint64_t wink_mcs51_virtual_us(void) {
    return s_virtual_us;
}

uint32_t wink_mcs51_quota_yield_count(void) {
    return s_quota_yields;
}

uint32_t wink_mcs51_master_tick_count(void) {
    // 100 Hz master tick = 10,000 us boundaries crossed by the slave clock
    // (1:1 conserved with master time billed via pal_os_busy_wait_us).
    return static_cast<uint32_t>(s_virtual_us / 10000u);
}

void wink_mcs51_charge_us(uint32_t us) {
    if (us == 0 || !in_fiber()) {
        return;
    }
    s_virtual_us += us;

    // Inside a virtual ISR: charge time (overflow-driven progress must stay
    // conserved) but never yield — re-entering the scheduler mid-ISR breaks
    // the catch-up path (ADR-0072 D4).
    if (wink_mcs51_in_isr()) {
        return;
    }

    if (static_cast<uint64_t>(s_virtual_us - s_slice_start) >= WINK_MCS51_QUOTA_US) {
        // Slice budget consumed: bill the whole slice 1:1 to the master,
        // hand control over (physics/UI drain), then run catch-up on resume.
        bill_master(static_cast<uint32_t>(s_virtual_us - s_slice_start));
        ++s_quota_yields;
        cooperative_yield();
        s_slice_start = s_virtual_us;
        do_catchup();
    }
}

// wink_mcs51_microstep() is defined in mcs51_bridge.cpp (boundary ④): it is
// the <intrins.h> _nop_() interception point and lives with the SFR hooks.

void wink_mcs51_delay_ms(uint32_t ms) {
    if (!in_fiber() || ms == 0) {
        return;
    }
    const uint64_t target = s_virtual_us + static_cast<uint64_t>(ms) * 1000u;
    while (s_virtual_us < target) {
        uint64_t remaining = target - s_virtual_us;
        uint32_t step = (remaining >= WINK_MCS51_QUOTA_US)
                            ? WINK_MCS51_QUOTA_US
                            : static_cast<uint32_t>(remaining);
        s_virtual_us += step;
        bill_master(step);
        ++s_quota_yields;
        cooperative_yield();
        s_slice_start = s_virtual_us;
        do_catchup();
    }
}

}  // extern "C"
