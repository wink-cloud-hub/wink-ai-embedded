// SPDX-License-Identifier: LGPL-3.0-only
// MCS-51 standard external interrupts: INT0 (vector 0) and INT1 (vector 2) —
// Stage 2 T3 (ADR-0076 A-class).
//
// Real 8051 hardware detects external activity asynchronously: with ITx=1
// (edge mode, TCON.0/TCON.2) a falling edge latches IEx (TCON.1/TCON.3); with
// ITx=0 (level mode) a low INT pin requests the interrupt for as long as it
// holds. Vectors are 0 (INT0) / 2 (INT1), gated by EA + EX0/EX1 (IE.7/IE.0/
// IE.2). Edge-mode IEx is hardware-cleared when the ISR is vectored.
//
// On classic parts the INT inputs are bond-fixed to P3.2 (linear pin 26) and
// P3.3 (pin 27). Enhanced families may route them through a pin-share mux:
// each line carries a generic selector-address slot (none = hardwired
// fallback above) that the owning chip package patches at init; an
// unprogrammed selector keeps the textbook mapping and a selector change
// drops the edge baseline (configuration event, never a synthetic edge).
// Full-port level-change interrupts are chip silicon modeled in the chip
// package, not here.
//
// The functional model samples the external pin level via the channel-1 read
// bridge (js_pal_gpio_read_state) at the microstep interception point, throttled
// to once per 10 ms virtual slice — the external world is frozen inside a
// slice and only changes at quota-yield boundaries, so sub-slice sampling
// cannot observe anything new and per-microstep JS calls would be pure
// overhead. A press/release shorter than one slice is invisible (documented
// degradation; small-appliance buttons far exceed 10 ms). Level mode re-requests
// at most once per slice while the pin is held low.
//
// Trap red lines: the poll is a pure state machine, takes zero simulated
// time, never yields, and READS the virtual clock for throttle only — it
// never advances it.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Sample INT0/INT1 external levels and dispatch per the ITx/IEx/EA/EXx rules.
// Called from the microstep interception point (fiber context); self-throttles
// to one sample per virtual slice.
void wink_mcs51_extint_poll(void);

// Reset model state (test isolation; called at framework init).
void wink_mcs51_extint_reset(void);

#ifdef __cplusplus
}  // extern "C"
#endif
