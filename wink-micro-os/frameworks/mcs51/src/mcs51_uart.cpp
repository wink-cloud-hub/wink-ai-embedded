// SPDX-License-Identifier: Apache-2.0
// MCS-51 UART functional model (M3, AD-2).
//
// See wink_mcs51_uart.h. SBUF write emits the byte to the console sink and the
// in-memory capture buffer, sets TI synchronously, and vectors UART ISR 4 when
// EA+ES are enabled (hardware does not auto-clear TI). All state is plain POD
// (zero-init BSS) — static-init safe (ADR-0072 D5).
#include "wink_mcs51_uart.h"

#include "mcs51_proxy.hpp"
#include "wink_mcs51_isr.h"

#include <cstdint>
#include <cstdio>

// Channel-2 live bridge (mirrors targets/wasm/wasm_bridge.h; the mcs51 layer
// declares it locally to stay free of axis-A PAL headers). Under emscripten
// this is a JS import (production wink_sim_js.js routes it to the UARTBus;
// the bounded Node stub logs it); on host mcs51_uni_bridge.cpp supplies a
// recording fallback. Port 0 = the 8051's single hardware UART.
extern "C" void js_pal_uart_write(uint8_t port, const uint8_t* buf, uint32_t len);

namespace {

constexpr uint8_t SFR_SCON = 0x98;
constexpr uint8_t SFR_SBUF = 0x99;
constexpr uint8_t SFR_IE   = 0xA8;

constexpr uint8_t SCON_TI = 1u;   // SCON.1 transmit-complete flag
constexpr uint8_t IE_ES   = 4u;   // IE.4 UART interrupt enable
constexpr uint8_t IE_EA   = 7u;   // IE.7 global interrupt enable

constexpr uint8_t  VECTOR_UART   = 4u;
constexpr uint32_t CAPTURE_CAP   = 4096u;

// Linear capture buffer (POD BSS). Bytes beyond capacity are dropped (the
// count saturates at CAPTURE_CAP) — tests emit a bounded, known sequence.
uint8_t  s_capture[CAPTURE_CAP] = {};
uint32_t s_count = 0;

void sfr_set_bit(uint8_t addr, uint8_t bit) {
    wink_mcs51_sfr_shadow[addr] |= static_cast<uint8_t>(1u << bit);
}

void on_sbuf_write(void) {
    uint8_t b = wink_mcs51_sfr_shadow[SFR_SBUF];

    // Console sink: plain putchar to stdout. Emscripten libc maps stdout to
    // Node fd 1, so the same call works on host and wasm; flush on newline so
    // the bounded node test captures output before exit.
    putchar(static_cast<int>(b));
    if (b == '\n') {
        fflush(stdout);
    }

    // In-memory capture for exact byte-sequence assertions (C ABI below).
    if (s_count < CAPTURE_CAP) {
        s_capture[s_count] = b;
        ++s_count;
    }

    // Live channel-2 route: SBUF write -> js_pal_uart_write -> PinArbiter
    // UARTBus (production) / recording fallback (host) / Node stub. Zero
    // simulated delay, same instant-complete semantics as TI below.
    js_pal_uart_write(0, &b, 1);

    // Transmit complete: latch TI synchronously, before returning, so the
    // classic `SBUF = c; while(!TI);` poll observes TI=1 on its first read.
    sfr_set_bit(SFR_SCON, SCON_TI);

    // Vector the UART ISR when EA+ES are enabled. Unlike the timer model,
    // hardware does NOT auto-clear TI/RI on vectoring — TI stays set for the
    // ISR (or polling code) to clear.
    uint8_t ie = wink_mcs51_sfr_shadow[SFR_IE];
    bool enabled = (ie & (1u << IE_EA)) && (ie & (1u << IE_ES));
    if (enabled) {
        (void)wink_mcs51_dispatch_vector(VECTOR_UART);
    }
}

}  // namespace

extern "C" {

void wink_mcs51_uart_on_write(uint8_t addr) {
    if (addr == SFR_SBUF) {
        on_sbuf_write();
    }
    // SCON write (including `TI = 0` bit clear) needs no model action: the
    // proxy already updated the shadow. Re-setting TI here would break the
    // software clear, so it is deliberately untouched.
}

void wink_mcs51_uart_on_read(uint8_t /*addr*/) {
    // No model side effect. SBUF/SCON reads are served entirely by the SFR
    // shadow (the proxy returns wink_mcs51_sfr_shadow[addr]); this hook exists
    // only so the bridge can route UART addresses symmetrically. Receive is
    // not modeled: a SBUF read simply returns whatever byte the shadow holds
    // (the last transmitted byte, or 0), and RI is shadow storage only.
}

void wink_mcs51_uart_reset(void) {
    s_count = 0;
    s_capture[0] = 0;  // not observable via the bounds-checked accessor
    // Clear the latched transmit-complete flag so a re-init starts with TI=0.
    wink_mcs51_sfr_shadow[SFR_SCON] &=
        static_cast<uint8_t>(~(1u << SCON_TI));
}

uint32_t wink_mcs51_uart_byte_count(void) {
    return s_count;
}

uint8_t wink_mcs51_uart_byte_at(uint32_t i) {
    if (i >= s_count) {
        return 0;
    }
    return s_capture[i];
}

}  // extern "C"
