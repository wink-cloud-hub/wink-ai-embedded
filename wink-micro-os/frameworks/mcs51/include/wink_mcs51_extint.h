// SPDX-License-Identifier: Apache-2.0
// MCS-51 external interrupt model: INT0 (vector 0) and INT1 (vector 2) —
// Stage 2 T3 (ADR-0076 A-class) with the CMS8S78xx pin-share mux.
//
// Real 8051 hardware detects external activity asynchronously: with ITx=1
// (edge mode, TCON.0/TCON.2) a falling edge latches IEx (TCON.1/TCON.3); with
// ITx=0 (level mode) a low INT pin requests the interrupt for as long as it
// holds. Vectors are 0 (INT0) / 2 (INT1), gated by EA + EX0/EX1 (IE.7/IE.0/
// IE.2). Edge-mode IEx is hardware-cleared when the ISR is vectored.
//
// On a classic 8051 the INT inputs are bond-fixed to P3.2 (linear pin 26) and
// P3.3 (pin 27). The CMS8S78xx routes them through a pin-share mux: PS_INT0
// (XSFR 0xF0C0) / PS_INT1 (XSFR 0xF0C1), value 0xPN = port P + pin N (ref
// manual §7.2.3; silicon reset 0x7F = no pin connected). The vendor EXTINT
// demo muxes INT0->P3.0 (pin 24) / INT1->P3.1 (pin 25). An unprogrammed or
// reserved selector falls back to the classic P3.2/P3.3 pins so generic-8051
// firmware keeps the textbook mapping; a selector change drops the edge
// baseline (configuration event, never a synthetic edge).
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
