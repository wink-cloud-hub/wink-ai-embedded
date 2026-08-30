// SPDX-License-Identifier: Apache-2.0
// MCS-51 UART functional model (M3, AD-2).
//
// Functional-level 8051 serial port: a write to SBUF (SFR 0x99) emits the byte
// to the simulated serial sink immediately (zero simulated transmission delay)
// and sets the transmit-complete flag TI (SCON.1, bit address 0x99) in the same
// call, BEFORE returning. This is the simplest correct semantics for the
// universal Keil transmit idiom:
//
//     SBUF = c;          // -> byte emitted, TI set synchronously
//     while(!TI);        // first read of TI observes 1; loop closes at once
//     TI = 0;            // software clears TI (bit write to SCON 0x98)
//
// There is NO UART timer/baud model (AD-2): the byte is not delayed, and the
// model never yields/blocks. When EA+ES (IE.7 / IE.4) are enabled the write
// vectors the UART ISR (vector 4); hardware does NOT auto-clear TI/RI on
// vectoring, so TI stays set until software clears it (mirrors the timer
// model's gating, not its auto-clear).
//
// The byte goes to three sinks: the host/wasm console (plain putchar to
// stdout; emscripten libc maps stdout to Node's fd 1), an in-memory capture
// buffer exposed over the C ABI so host/wasm tests can assert exact byte
// sequences without parsing stdout, and the live channel-2 route
// (js_pal_uart_write -> PinArbiter UARTBus in production).
//
// Receive (Stage 2, ADR-0076 A-class): external bytes enter via
// wink_mcs51_uart_rx_push() (host test injection; emscripten KEEPALIVE export
// for the UARTBus plugin). Bytes queue in a small PENDING FIFO and are drained
// at microstep interception points ON THE FIBER CONTEXT — a push from the JS
// side never re-enters the firmware directly. Drain honors SCON.REN (bytes
// stay pending while the receiver is disabled), latches RI (SCON.0), writes
// the SBUF shadow, and vectors UART ISR 4 when EA+ES are gated. Hardware does
// NOT auto-clear RI on vectoring — the ISR (or polling code) clears it; a new
// byte arriving while RI is still set is dropped (saturating counter), which
// mirrors the no-FIFO 8051 overflow behavior.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// SFR hook entry points (invoked by the SFR proxy via the bridge).
// SBUF write: emit the byte to the console + capture buffer, set TI, and
// vector UART ISR (4) when EA+ES are gated. SCON writes do nothing special —
// the shadow already holds the value (software clears TI via a bit write).
void wink_mcs51_uart_on_write(uint8_t addr);
// SBUF/SCON read hook: a no-op. Reads are served entirely by the SFR shadow
// (the proxy returns the shadow byte); the model does not drive receive, so
// there is no side effect. The bridge still calls it for symmetric routing.
void wink_mcs51_uart_on_read(uint8_t addr);

// Reset UART model state (test isolation; called at framework init): zero the
// capture-buffer length, clear the latched TI/RI bits in the SCON shadow, and
// flush the RX pending FIFO + drop counter, so a re-init starts clean.
void wink_mcs51_uart_reset(void);

// Receive injection entry (channel-2 plugin -> firmware; called from OUTSIDE
// the fiber — host test harness or the emscripten-exported UARTBus callback).
// The byte is queued (POD BSS FIFO) and later drained on the fiber context at
// a microstep interception point via wink_mcs51_uart_rx_drain() — never
// re-enters firmware from the caller. Queue-full bytes are dropped (saturating
// counter, observable via wink_mcs51_uart_rx_dropped()).
void wink_mcs51_uart_rx_push(uint8_t byte);

// Fiber-context drain: deliver pending RX bytes per the REN/RI/EA+ES rules
// (called from the microstep interception point). Pure state machine, zero
// simulated time.
void wink_mcs51_uart_rx_drain(void);

// Saturating count of RX bytes lost to overflow (FIFO full, or byte completed
// while RI was still set — the 8051 has no hardware RX FIFO).
uint32_t wink_mcs51_uart_rx_dropped(void);

// ── Test observability (C ABI) ──────────────────────────────────────────────
// Number of bytes captured since reset (capped at the buffer capacity).
uint32_t wink_mcs51_uart_byte_count(void);
// The i-th captured byte (0-based). Indices >= byte_count() read as 0.
uint8_t wink_mcs51_uart_byte_at(uint32_t i);

#ifdef __cplusplus
}  // extern "C"
#endif
