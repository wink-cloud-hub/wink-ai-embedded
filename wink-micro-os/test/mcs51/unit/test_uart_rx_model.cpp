// SPDX-License-Identifier: Apache-2.0
// Stage 2 T2 (ADR-0076 A-class): UART RX model — external bytes pushed via
// wink_mcs51_uart_rx_push() queue in the pending FIFO and are delivered by
// wink_mcs51_uart_rx_drain() (called from the microstep interception point on
// the fiber context). Hardware rules verified:
//   * REN (SCON.4) gates reception — bytes stay queued while REN=0;
//   * delivery latches RI (SCON.0), writes the SBUF shadow, and vectors the
//     UART ISR (4) only when EA+ES are both set;
//   * RI is NOT auto-cleared by hardware: the ISR clears it; a byte completing
//     while RI is still set is lost (no RX FIFO on an 8051) -> dropped count;
//   * TX/RX SBUF shadow interleaving: TX captures its byte at write time, so a
//     later RX overwrite cannot corrupt transmission.
#include <stdint.h>
#include <stdio.h>

#include "mcs51_proxy.hpp"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_isr.h"
#include "wink_mcs51_uart.h"

namespace {

constexpr uint8_t SFR_SBUF    = 0x99;
constexpr uint8_t SFR_SCON    = 0x98;
constexpr uint8_t SFR_IE      = 0xA8;
constexpr uint8_t SCON_RI_BIT = 0u;
constexpr uint8_t SCON_TI_BIT = 1u;
constexpr uint8_t SCON_REN_BIT = 4u;
constexpr uint8_t IE_ES_BIT   = 4u;
constexpr uint8_t IE_EA_BIT   = 7u;

uint32_t g_isr_hits = 0;
uint8_t  g_rx[64];
uint32_t g_rx_count = 0;

uint8_t ri_bit(void) {
    return static_cast<uint8_t>(
        (wink_mcs51_sfr_shadow[SFR_SCON] >> SCON_RI_BIT) & 1u);
}

void configure(bool ren, bool es, bool ea) {
    uint8_t scon = 0x40u;  // mode 1 (8-bit UART)
    if (ren) scon |= (1u << SCON_REN_BIT);
    wink_mcs51_sfr_shadow[SFR_SCON] = scon;
    uint8_t ie = 0;
    if (es) ie |= (1u << IE_ES_BIT);
    if (ea) ie |= (1u << IE_EA_BIT);
    wink_mcs51_sfr_shadow[SFR_IE] = ie;
}

}  // namespace

// UART ISR: standard Keil shape — on RI, read SBUF and clear RI; on TI, clear
// TI. Hardware vectors here for both flags on vector 4.
WINK_ISR(4) {
    uint8_t scon = wink_mcs51_sfr_shadow[SFR_SCON];
    if (scon & (1u << SCON_RI_BIT)) {
        if (g_rx_count < sizeof(g_rx)) {
            g_rx[g_rx_count] = wink_mcs51_sfr_shadow[SFR_SBUF];
        }
        ++g_rx_count;
        wink_mcs51_sfr_shadow[SFR_SCON] &=
            static_cast<uint8_t>(~(1u << SCON_RI_BIT));
    }
    if (scon & (1u << SCON_TI_BIT)) {
        wink_mcs51_sfr_shadow[SFR_SCON] &=
            static_cast<uint8_t>(~(1u << SCON_TI_BIT));
    }
    ++g_isr_hits;
}

// The bridge TU references the user entry; this test drives the model
// directly, so an empty definition closes the link.
extern "C" void wink_mcs51_user_main(void) {}

extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

#define CHECK(cond, msg)                                          \
    do {                                                          \
        if (!(cond)) {                                            \
            printf("[mcs51-rx] FAIL: %s (line %d)\n", msg, __LINE__); \
            ++fails;                                              \
        }                                                         \
    } while (0)

int main(void) {
    int fails = 0;

    wink_mcs51_isr_enable();
    wink_mcs51_uart_reset();  // TI=RI=0, FIFO flushed
    g_isr_hits = 0;
    g_rx_count = 0;

    // ── A: REN + EA + ES → pushed bytes deliver via ISR, in order, paced at
    //    the functional wire byte time (1 ms) so the mailbox keeps up ────────
    configure(/*ren=*/true, /*es=*/true, /*ea=*/true);
    wink_mcs51_uart_rx_push('A');
    wink_mcs51_uart_rx_push('B');
    wink_mcs51_uart_rx_push('C');
    wink_mcs51_uart_rx_drain();  // first byte lands immediately
    CHECK(g_isr_hits == 1 && g_rx_count == 1 && g_rx[0] == 'A',
          "A: first byte delivered immediately");
    wink_mcs51_test_advance_virtual_us(1000);
    wink_mcs51_uart_rx_drain();  // second byte after one wire byte time
    CHECK(g_isr_hits == 2 && g_rx_count == 2 && g_rx[1] == 'B',
          "A: second byte delivered after 1 ms");
    wink_mcs51_test_advance_virtual_us(1000);
    wink_mcs51_uart_rx_drain();  // third
    CHECK(g_isr_hits == 3, "A: 3 bytes -> 3 vector-4 dispatches");
    CHECK(g_rx_count == 3 && g_rx[2] == 'C',
          "A: ISR received exact byte sequence");
    CHECK(ri_bit() == 0, "A: RI cleared by ISR after each byte");
    CHECK(wink_mcs51_uart_rx_dropped() == 0, "A: no dropped bytes");

    // ── B: REN=0 → bytes stay queued; enabling REN then delivers ────────────
    g_isr_hits = 0;
    g_rx_count = 0;
    configure(/*ren=*/false, /*es=*/true, /*ea=*/true);
    wink_mcs51_uart_rx_push('X');
    wink_mcs51_uart_rx_drain();
    CHECK(g_isr_hits == 0, "B: no delivery while REN=0");
    CHECK(ri_bit() == 0, "B: RI stays clear while REN=0");
    wink_mcs51_test_advance_virtual_us(1000);  // wire byte time passes...
    configure(/*ren=*/true, /*es=*/true, /*ea=*/true);
    wink_mcs51_uart_rx_drain();  // ...still queued, delivers now
    CHECK(g_isr_hits == 1 && g_rx_count == 1 && g_rx[0] == 'X',
          "B: queued byte delivers after REN enabled");

    // ── C: ES=0 → RI latches for polling, no vector ─────────────────────────
    g_isr_hits = 0;
    g_rx_count = 0;
    configure(/*ren=*/true, /*es=*/false, /*ea=*/true);
    wink_mcs51_uart_rx_push('P');
    wink_mcs51_test_advance_virtual_us(1000);
    wink_mcs51_uart_rx_drain();
    CHECK(g_isr_hits == 0, "C: no vector when ES=0 (polled RX)");
    CHECK(ri_bit() == 1, "C: RI latched for polling path");
    CHECK(wink_mcs51_sfr_shadow[SFR_SBUF] == 'P', "C: SBUF holds received byte");

    // ── D: slow consumer (RI never cleared) → second byte dropped ───────────
    g_isr_hits = 0;
    g_rx_count = 0;
    configure(/*ren=*/true, /*es=*/false, /*ea=*/true);  // RI stays set
    wink_mcs51_sfr_shadow[SFR_SCON] |= (1u << SCON_RI_BIT);  // RI already pending
    uint32_t dropped_before = wink_mcs51_uart_rx_dropped();
    wink_mcs51_uart_rx_push('Q');
    wink_mcs51_test_advance_virtual_us(1000);
    wink_mcs51_uart_rx_drain();
    CHECK(wink_mcs51_uart_rx_dropped() == dropped_before + 1,
          "D: byte completing with RI set is dropped (no HW FIFO)");

    // ── E: TX/RX shadow interleave — TX capture unaffected by later RX ──────
    g_isr_hits = 0;
    g_rx_count = 0;
    configure(/*ren=*/true, /*es=*/true, /*ea=*/true);
    uint32_t cap_before = wink_mcs51_uart_byte_count();
    wink_mcs51_sfr_shadow[SFR_SBUF] = static_cast<uint8_t>('T');
    wink_mcs51_uart_on_write(SFR_SBUF);  // TX: console + capture + ch2 route
    CHECK(wink_mcs51_uart_byte_count() == cap_before + 1,
          "E: TX captured one byte");
    CHECK(wink_mcs51_uart_byte_at(cap_before) == 'T',
          "E: captured byte is 'T'");
    // The TX ISR (vector 4) clears TI; then an RX byte overwrites the shadow
    // after one wire byte time.
    g_isr_hits = 0;
    g_rx_count = 0;
    wink_mcs51_test_advance_virtual_us(1000);
    wink_mcs51_uart_rx_push('R');
    wink_mcs51_uart_rx_drain();
    // Pacing: R is the first delivery after the TX path; subsequent echo-style
    // bytes need virtual time to pass, but this single byte lands immediately.
    CHECK(g_isr_hits == 1 && g_rx_count == 1 && g_rx[0] == 'R',
          "E: RX ISR read 'R' from SBUF shadow");
    CHECK(g_rx_count == 1 && g_rx[0] == 'R',
          "E: RX ISR read 'R' from SBUF shadow");
    CHECK(wink_mcs51_uart_byte_at(cap_before) == 'T',
          "E: TX capture still 'T' after RX shadow overwrite");

    if (fails) {
        return 1;
    }
    printf("[mcs51-rx] PASS: UART RX model — REN gate, RI latch + vector 4, "
           "no-FIFO drop, TX/RX shadow interleave\n");
    return 0;
}
