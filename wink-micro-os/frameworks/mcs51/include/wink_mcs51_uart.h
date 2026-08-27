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
// The byte goes to two sinks: the host/wasm console (plain putchar to stdout;
// emscripten libc maps stdout to Node's fd 1) and an in-memory capture buffer
// exposed over the C ABI so host/wasm tests can assert exact byte sequences
// without parsing stdout.
//
// Receive is modeled minimally: reading SBUF returns the last transmitted byte
// (or 0); RI is shadow storage only. No receive ring buffer (YAGNI).
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
// SBUF/SCON read: no model side effect (microstep is charged by the bridge).
void wink_mcs51_uart_on_read(uint8_t addr);

// Reset UART model state + capture buffer (test isolation; framework init).
void wink_mcs51_uart_reset(void);

// ── Test observability (C ABI) ──────────────────────────────────────────────
// Number of bytes captured since reset (capped at the buffer capacity).
uint32_t wink_mcs51_uart_byte_count(void);
// The i-th captured byte (0-based). Indices >= byte_count() read as 0.
uint8_t wink_mcs51_uart_byte_at(uint32_t i);

#ifdef __cplusplus
}  // extern "C"
#endif
