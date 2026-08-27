// SPDX-License-Identifier: Apache-2.0
// MCS-51 Timer0/Timer1 functional model (M2, AD-2 / ADR-0072).
//
// Functional-level timing: the standard teaching convention of a 12 MHz
// crystal is used, i.e. one timer count equals one virtual microsecond (the
// 12-T machine-cycle prescaler is NOT modeled instruction-by-instruction;
// reload values computed from µs periods — the universal Keil idiom,
// e.g. TH0=0x3C/TL0=0xB0 for 50 ms — therefore produce exact µs periods).
//
// Time progress is driven two ways, both on the fiber:
//   * eagerly from the clock catch-up hook at every quota/delay resume
//     (so an ISR-driven program that never polls TF still gets overflows),
//   * lazily from a TCON read hook (the `while(!TF0);` busy-wait closes the
//     loop on the read itself).
// Overflow sets TFx in the TCON shadow and vectors the timer ISR (vector 1
// / 3) when EA and ETx are enabled; hardware clears TFx on vectoring. A
// polled TFx (no ISR / interrupts disabled) stays set for software to clear.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// SFR hook entry points (invoked by the SFR proxy via the bridge).
// Evaluate pending overflows up to the current virtual time before a TCON
// read, so a polling read observes the current TFx state.
void wink_mcs51_timer_on_read(uint8_t addr);
// Latch TMOD/TH/TL writes and handle TR0/TR1 start/stop (TCON write).
void wink_mcs51_timer_on_write(uint8_t addr);

// Catch-up entry: advance both timers to virtual time `now_us`, firing
// overflows (TF set + ISR dispatch) as they fall due. Registered as the
// clock catch-up hook at framework init.
void wink_mcs51_timers_step_to(uint64_t now_us);

// Reset timer state (test isolation).
void wink_mcs51_timers_reset(void);

#ifdef __cplusplus
}  // extern "C"
#endif
