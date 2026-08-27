// SPDX-License-Identifier: Apache-2.0
// M3 UART review test: the EA+ES-gated UART ISR path (vector 4).
//
// The uart_printf sample is polled (it never sets EA/ES), so the vector-4
// dispatch branch in mcs51_uart.cpp is otherwise untested. This framework-side
// unit test drives the model hook directly (no fiber/runtime needed):
//   * registers a vector-4 handler via the WINK_ISR auto-registration shim;
//   * with IE EA(7)+ES(4) set, a SBUF write must dispatch vector 4 exactly once
//     AND leave TI set (8051 hardware does NOT auto-clear TI/RI on vectoring);
//   * with EA/ES clear, a further SBUF write must NOT dispatch, while TI is
//     still latched for the polling path.
#include <stdint.h>
#include <stdio.h>

#include "mcs51_proxy.hpp"
#include "wink_mcs51_isr.h"
#include "wink_mcs51_uart.h"

namespace {

constexpr uint8_t SFR_SBUF     = 0x99;
constexpr uint8_t SFR_SCON     = 0x98;
constexpr uint8_t SFR_IE       = 0xA8;
constexpr uint8_t SCON_TI_BIT  = 1u;
constexpr uint8_t IE_ES_BIT    = 4u;
constexpr uint8_t IE_EA_BIT    = 7u;
constexpr uint8_t VECTOR_UART  = 4u;

uint32_t g_uart_isr_hits = 0;

uint8_t ti_bit(void) {
    return static_cast<uint8_t>(
        (wink_mcs51_sfr_shadow[SFR_SCON] >> SCON_TI_BIT) & 1u);
}

}  // namespace

WINK_ISR(4) {
    ++g_uart_isr_hits;
}

// The bridge TU (pulled in transitively via the SFR shadow/hooks the compat
// lib carries) references the user entry; this test drives the model directly,
// so an empty definition closes the link (mirrors test_static_init_safety).
extern "C" void wink_mcs51_user_main(void) {}

extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

int main(void) {
    int fails = 0;

    // Open the execution-phase interrupt gate (static registration already
    // happened at load; dispatch is suppressed until enabled).
    wink_mcs51_isr_enable();
    wink_mcs51_uart_reset();  // start with TI=0 and empty capture

    // ── Gated: EA+ES enabled → SBUF write vectors UART ISR (4) ──────────────
    wink_mcs51_sfr_shadow[SFR_IE] =
        static_cast<uint8_t>((1u << IE_EA_BIT) | (1u << IE_ES_BIT));
    wink_mcs51_sfr_shadow[SFR_SBUF] = static_cast<uint8_t>('A');
    wink_mcs51_uart_on_write(SFR_SBUF);

    uint32_t dispatched = wink_mcs51_isr_dispatch_count(VECTOR_UART);
    if (dispatched != 1u || g_uart_isr_hits != 1u) {
        printf("[mcs51] FAIL: gated SBUF write: vector 4 dispatched %u times "
               "(hits %u), want 1\n",
               (unsigned)dispatched, (unsigned)g_uart_isr_hits);
        fails++;
    }
    // Hardware does NOT auto-clear TI on vectoring (unlike TFx for timers).
    if (ti_bit() != 1u) {
        printf("[mcs51] FAIL: TI was cleared by vectoring (must stay set)\n");
        fails++;
    }

    // ── Ungated: EA/ES clear → another SBUF write must NOT vector ───────────
    wink_mcs51_sfr_shadow[SFR_IE] = 0u;
    // Emulate the polling idiom's software TI clear, then write again.
    wink_mcs51_sfr_shadow[SFR_SCON] &=
        static_cast<uint8_t>(~(1u << SCON_TI_BIT));
    wink_mcs51_sfr_shadow[SFR_SBUF] = static_cast<uint8_t>('B');
    wink_mcs51_uart_on_write(SFR_SBUF);

    dispatched = wink_mcs51_isr_dispatch_count(VECTOR_UART);
    if (dispatched != 1u || g_uart_isr_hits != 1u) {
        printf("[mcs51] FAIL: ungated SBUF write dispatched vector 4 "
               "(count %u, hits %u), want it to stay 1\n",
               (unsigned)dispatched, (unsigned)g_uart_isr_hits);
        fails++;
    }
    // TI is still latched by the write even with no vector (polled path).
    if (ti_bit() != 1u) {
        printf("[mcs51] FAIL: TI not set on ungated SBUF write\n");
        fails++;
    }

    if (fails) {
        return 1;
    }
    printf("[mcs51] PASS: UART vector-4 gated by EA+ES (dispatched x1 when "
           "enabled, TI stays set after vector; no dispatch when disabled)\n");
    return 0;
}
