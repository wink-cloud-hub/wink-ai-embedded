// SPDX-License-Identifier: Apache-2.0
// MCS-51 virtual slave clock + cooperative quota engine (ADR-0072 D1~D3).
//
// The 51 user program assumes it owns the CPU: `while(!TF0);` is a bare-metal
// poll, `delay_ms()` is a busy wait, and Timer0 overflows are measured in
// microseconds. Under the cooperative fiber scheduler a loop that never
// blocks would freeze the host (the WCET 8002 fault is post-hoc and cannot
// recover a frozen fiber). This module implements the dual-clock-domain
// contract:
//
//   * s_virtual_us is the mcs51 slave clock, measured in simulated 8051
//     microseconds. It advances ONLY via interception-point charging inside
//     the fiber (microsteps and delays). It must never be advanced by the
//     master after a yield — the fiber's own quota check depends on it
//     (chicken-and-egg, Spike-S1 §3.1).
//   * 1 virtual ms is billed to the master 1:1 (AD-14): each consumed quota
//     slice advances the platform virtual clock (host s_time_us / wasm
//     pal_wasm virtual clock) by the same amount via pal_os_busy_wait_us,
//     so UniSim physical integration and the 51 timers stay in lockstep.
//   * When a slice consumes QUOTA_US of virtual time, the fiber performs a
//     duration-0 cooperative yield (pal_os_sleep_ms(0)) to hand control to the
//     master; on resume it performs catch-up bookkeeping (timer stepping),
//     so tight idle polls never freeze simulation and virtual time is
//     conserved 1:1 with master ticks (Catch-Up rule, AD-17 / D3).
//
// Instantaneous peripheral traps (ADC0832, CMS8S ADC) cost 0us: they are
// state transitions, not waits (AD-2 functional level, D1).
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Functional microseconds charged per interception point (an intercepted SFR
// access or a `_nop_()`). Approximates a few 12-T machine cycles at 12 MHz;
// it is the granularity of virtual-time progress inside tight loops.
#define WINK_MCS51_MICROSTEP_US 5u

// Virtual-time slice budget, aligned to one 100 Hz master tick (10,000 us):
// the production runtime (pal_sim_scheduler_run) counts one fiber dispatch as
// one tick, so a quota yield hands back exactly one master tick and the slave
// clock stays 1:1 with tick count (delay_ms(100) == 10 ticks, SSOT §6.2).
// The Spike-S1 PoC used a custom master loop counting time boundaries and
// proposed a 500 us slice; under the real scheduler that would bill 10 ms of
// master time per 500 us slice and break conservation. Orthogonal to the
// wall-clock WCET threshold (5,000 us), the post-hoc freeze backstop
// (Spike-S1 §3.5).
#define WINK_MCS51_QUOTA_US 10000u

// Framework lifecycle: reset all slave-clock state (test isolation). Called
// before the fiber is entered; safe to call multiple times.
void wink_mcs51_clock_reset(void);

// Charge `us` virtual microseconds to the slave clock. If the running slice
// exceeds the quota, perform a duration-0 cooperative yield and, on resume,
// run catch-up bookkeeping. Called from every interception point.
//
// ISR context (wink_mcs51_in_isr()) charges time but never yields: ISRs run
// inside the catch-up/overflow path, where re-entering the scheduler would
// deadlock (Trap red line 2, ADR-0072 D4).
void wink_mcs51_charge_us(uint32_t us);

// Interception-point entry used by <intrins.h> _nop_() and the SFR proxy.
// Equivalent to wink_mcs51_charge_us(WINK_MCS51_MICROSTEP_US).
void wink_mcs51_microstep(void);

// User-visible delay rail: Keil projects bring their own `delay_ms(n)` busy
// wait (it burns microsteps through SFR/NOP interception), but a delay helper
// is also exposed for samples and tests that want exact virtual-time sleeps.
// Bills `ms` of virtual time 1:1 and yields the fiber until that virtual
// deadline; remaining master time is advanced 1:1.
void wink_mcs51_delay_ms(uint32_t ms);

// Current slave clock, in virtual microseconds.
uint64_t wink_mcs51_virtual_us(void);

// Observability (tests / acceptance #2).
uint32_t wink_mcs51_quota_yield_count(void);
uint32_t wink_mcs51_master_tick_count(void);

// Catch-up hook: invoked on the fiber side each time the fiber resumes after a
// quota yield (or after a delay sleep), with the slave clock advanced to
// `now_us`. Timer models hook here to step counters and dispatch overflows
// (AD-17). Registered by mcs51_timer.cpp; NULL is tolerated.
typedef void (*wink_mcs51_catchup_fn_t)(uint64_t now_us);
void wink_mcs51_set_catchup_hook(wink_mcs51_catchup_fn_t hook);

#ifdef __cplusplus
}  // extern "C"
#endif
