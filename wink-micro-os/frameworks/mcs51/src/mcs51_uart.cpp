// SPDX-License-Identifier: Apache-2.0
// MCS-51 UART functional model (M3, AD-2).
//
// See wink_mcs51_uart.h. SBUF write emits the byte to the console sink and the
// in-memory capture buffer, sets TI synchronously, and vectors UART ISR 4 when
// EA+ES are enabled (hardware does not auto-clear TI). All state is plain POD
// (zero-init BSS) — static-init safe (ADR-0072 D5).
#include "wink_mcs51_uart.h"

#include "mcs51_proxy.hpp"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_isr.h"

#include <cstdint>
#include <cstdio>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

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
constexpr uint8_t SCON_RI = 0u;   // SCON.0 receive-complete flag
constexpr uint8_t SCON_REN = 4u;  // SCON.4 receive enable
constexpr uint8_t IE_ES   = 4u;   // IE.4 UART interrupt enable
constexpr uint8_t IE_EA   = 7u;   // IE.7 global interrupt enable

constexpr uint8_t  VECTOR_UART   = 4u;
constexpr uint32_t CAPTURE_CAP   = 4096u;
constexpr uint32_t RX_FIFO_CAP   = 64u;
// Functional byte-arrival spacing: on real hardware receive-complete events
// are separated by the wire byte time (~1.04 ms at 9600 8N1, the near-universal
// small-appliance rate). Pacing deliveries at >= 1 ms virtual gives the
// firmware's ISR/poll a chance to consume each byte (its one-deep hardware
// mailbox) before the next lands, instead of all queued bytes vectoring inside
// one microstep. Pure read-time evaluation of the virtual clock — never
// advances it (trap red lines). Bytes pushed faster queue in the FIFO.
constexpr uint64_t RX_BYTE_SPACING_US = 1000ull;

// Linear capture buffer (POD BSS). Bytes beyond capacity are dropped (the
// count saturates at CAPTURE_CAP) — tests emit a bounded, known sequence.
uint8_t  s_capture[CAPTURE_CAP] = {};
uint32_t s_count = 0;

// RX pending FIFO (POD BSS). External bytes are pushed from outside the fiber
// (host harness / JS UARTBus callback) and drained on the fiber context at
// microstep points. 8051 hardware has no RX FIFO: a byte completing while RI
// is still set is lost — modeled by s_rx_dropped (saturating).
uint8_t  s_rx_fifo[RX_FIFO_CAP] = {};
uint32_t s_rx_head = 0;
uint32_t s_rx_tail = 0;
uint32_t s_rx_dropped = 0;
uint64_t s_rx_last_deliver_us = 0;
bool     s_rx_have_delivered = false;

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

// Deliver one pending RX byte per the hardware rules. Returns true while more
// bytes may be deliverable. Pure state machine; runs on the fiber context.
bool rx_deliver_one(void) {
    if (s_rx_tail == s_rx_head) {
        return false;  // FIFO empty
    }
    uint8_t scon = wink_mcs51_sfr_shadow[SFR_SCON];
    if ((scon & (1u << SCON_REN)) == 0) {
        return false;  // receiver disabled: bytes stay queued until REN=1
    }
    // Functional arrival pacing: space deliveries >= one wire byte time so the
    // firmware's one-deep mailbox can keep up (see RX_BYTE_SPACING_US).
    uint64_t now = wink_mcs51_virtual_us();
    if (s_rx_have_delivered &&
        (now - s_rx_last_deliver_us) < RX_BYTE_SPACING_US) {
        return false;  // too early; remaining bytes land on later microsteps
    }
    uint8_t b = s_rx_fifo[s_rx_tail % RX_FIFO_CAP];
    s_rx_tail++;
    if (scon & (1u << SCON_RI)) {
        // Previous byte never read (no hardware FIFO): this byte is lost.
        if (s_rx_dropped < 0xFFFFFFFFu) {
            ++s_rx_dropped;
        }
        return s_rx_tail != s_rx_head;
    }
    // SBUF reads return the RX register on real hardware; the single shadow
    // holds the received byte for firmware reads (TX captures its value at
    // write time, so this overwrite cannot corrupt transmission).
    wink_mcs51_sfr_shadow[SFR_SBUF] = b;
    sfr_set_bit(SFR_SCON, SCON_RI);
    s_rx_last_deliver_us = now;
    s_rx_have_delivered = true;

    uint8_t ie = wink_mcs51_sfr_shadow[SFR_IE];
    bool enabled = (ie & (1u << IE_EA)) && (ie & (1u << IE_ES));
    if (enabled) {
        (void)wink_mcs51_dispatch_vector(VECTOR_UART);
    }
    return s_rx_tail != s_rx_head;
}

}  // namespace

extern "C" {

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void wink_mcs51_uart_rx_push(uint8_t byte) {
    uint32_t used = s_rx_head - s_rx_tail;
    if (used >= RX_FIFO_CAP) {
        if (s_rx_dropped < 0xFFFFFFFFu) {
            ++s_rx_dropped;
        }
        return;
    }
    s_rx_fifo[s_rx_head % RX_FIFO_CAP] = byte;
    ++s_rx_head;
}

void wink_mcs51_uart_rx_drain(void) {
    // Bound the drain loop so a push storm between microsteps cannot stall
    // the fiber: deliver at most FIFO capacity bytes per interception point;
    // the remainder drains on subsequent microsteps.
    for (uint32_t i = 0; i < RX_FIFO_CAP; ++i) {
        if (!rx_deliver_one()) {
            break;
        }
    }
}

uint32_t wink_mcs51_uart_rx_dropped(void) {
    return s_rx_dropped;
}

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
    // Clear the latched TX/RX flags so a re-init starts with TI=RI=0, and
    // flush the RX pending FIFO + drop counter.
    wink_mcs51_sfr_shadow[SFR_SCON] &=
        static_cast<uint8_t>(~((1u << SCON_TI) | (1u << SCON_RI)));
    s_rx_head = 0;
    s_rx_tail = 0;
    s_rx_dropped = 0;
    s_rx_have_delivered = false;
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
