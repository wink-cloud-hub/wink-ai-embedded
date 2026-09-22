// SPDX-License-Identifier: GPL-3.0-only
// T1.2: CMS8S78xx on-chip SPI master model unit tests
// (PLAN-20260921-CMS8S78XX-I2C-SPI-DEADLOCK §5.2).
//
// Locks the Phase 1 contract:
//   * SPDR write completes synchronously: the vendor SPI_Transmit poll
//     (`while(!SPI_GetTransferIntFlag())`) exits on the first read (no
//     simulation deadlock);
//   * standard two-step read-clear (read SPSR -> read SPDR); a lone SPDR
//     read must not clear the pending flag;
//   * SPISIF/WCOL are hardware-owned: software SPSR writes land only in
//     SSCEN; WCOL sets on overwrite-before-consume and shares the clear pair;
//   * per-byte charge = ceil(8 / (Fsys/SPIClkDiv)) from the clock model;
//   * SSCR.NSSO1 edges are recorded as frame start/end;
//   * EIE2.SPIIE latches EIF2.SPIIF and dispatches vector 22 (SW-clear);
//   * reset restores NSSx deasserted (0x02), 0xFF mock RX and zero counters,
//     and re-installs all hooks (S4-H2).
#include <stdint.h>
#include <stdio.h>

#include "cms8s78xx.h"
#include "cms8s_spi.h"
#include "mcs51_context.h"
#include "mcs51_test_harness.h"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_isr.h"

#undef main
#undef printf
extern "C" void wink_mcs51_user_main(void) {}
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

// Host-side channel-2 SPI fallback (Phase 2 prerequisite): must link and
// fail closed on host (no JS bus data plane).
extern "C" bool js_pal_spi_transfer(uint8_t port, uint16_t device_id,
                                    const uint8_t* tx_buf, uint32_t len,
                                    uint8_t* rx_buf, uint8_t mode,
                                    uint32_t sck_hz);

namespace {

int g_fails = 0;
uint32_t g_spi_isr_hits = 0;

void check(bool cond, const char* msg) {
    if (!cond) {
        printf("[cms8s_spi] FAIL: %s\n", msg);
        ++g_fails;
    }
}

constexpr uint8_t SPISIF = 0x80u;
constexpr uint8_t WCOL   = 0x40u;
constexpr uint8_t SSCEN  = 0x01u;
constexpr uint8_t SPCR_DIV_4  = 0x00u;
constexpr uint8_t SPCR_DIV_8  = 0x01u;
constexpr uint8_t SPCR_DIV_64 = 0x20u;
constexpr uint8_t EIE2_SPIIE  = 0x80u;
constexpr uint8_t EIF2_SPIIF  = 0x80u;
constexpr uint8_t IE_EA       = 0x80u;
constexpr uint8_t VECTOR_SPI  = 22u;

// Vendor demo_spi.c SPI_Transmit verbatim against the shim proxies (the
// vendor parameter is `Data`; `data` is a Keil memory-space macro, so the
// local name must not collide with the dialect erasure).
uint8_t vendor_spi_transmit(uint8_t tx) {
    SPDR = tx;
    while (!(SPSR & SPI_SPSR_SPISIF_Msk)) {
    }
    return static_cast<uint8_t>(SPDR);
}

// Consumes a pending completion through the standard read-clear pair.
void consume_flags(void) {
    (void)static_cast<uint8_t>(SPSR);
    (void)static_cast<uint8_t>(SPDR);
}

}  // namespace

WINK_ISR(22) {
    ++g_spi_isr_hits;
}

int main(void) {
    printf("[cms8s_spi] Starting CMS8S78xx SPI unit tests...\n");

    mcs51_test_register_family(MCS51_FAMILY_CMS8S78XX);
    mcs51_context_set_family(MCS51_FAMILY_CMS8S78XX);
    Mcu51Context* ctx = mcs51_get_context();
    mcs51_context_reset(ctx);
    ctx->clock_hz = 12000000u;
    ctx->virtual_us = 0;
    wink_mcs51_isr_enable();
    cms8s_spi_init(ctx);

    // ── 1) Reset state ──────────────────────────────────────────────────────
    check(static_cast<uint8_t>(SSCR) == 0x02u,
          "SSCR reset value must be 0x02 (NSSx deasserted)");
    check((static_cast<uint8_t>(SPSR) & (SPISIF | WCOL)) == 0u,
          "SPISIF/WCOL must be clear after reset");
    check(cms8s_spi_transfer_count() == 0u, "transfer count must start at 0");
    check(cms8s_spi_frame_start_count() == 0u &&
              cms8s_spi_frame_end_count() == 0u,
          "frame edge counters must start at 0");
    check(cms8s_spi_rx_value() == 0xFFu, "mock RX must default to 0xFF");

    // ── 2) Synchronous completion through the vendor idiom ─────────────────
    const uint8_t rx = vendor_spi_transmit(0xA5u);
    check(rx == 0xFFu, "default mock RX must be 0xFF (MISO idle high)");
    check(cms8s_spi_transfer_count() == 1u,
          "transfer count after transmit != 1");
    check(cms8s_spi_last_tx() == 0xA5u, "last TX byte != 0xA5");
    check((static_cast<uint8_t>(SPSR) & SPISIF) == 0u,
          "vendor transmit idiom did not clear SPISIF");

    // ── 3) Two-step read-clear: SPSR arms, SPDR clears ──────────────────────
    SPDR = 0x11u;
    {
        const uint8_t first = static_cast<uint8_t>(SPDR);  // lone SPDR read
        check(first == 0xFFu, "RX read != 0xFF");
        check((static_cast<uint8_t>(SPSR) & SPISIF) != 0u,
              "lone SPDR read must not clear SPISIF");
    }
    consume_flags();
    check((static_cast<uint8_t>(SPSR) & SPISIF) == 0u,
          "SPSR->SPDR sequence did not clear SPISIF");

    // ── 4) SPSR software write: only SSCEN is writable ──────────────────────
    SPSR = 0xFFu;
    check((static_cast<uint8_t>(SPSR) & (SPISIF | WCOL)) == 0u,
          "software SPSR write must not set SPISIF/WCOL");
    check((static_cast<uint8_t>(SPSR) & SSCEN) != 0u,
          "software SSCEN set lost");
    SPSR = 0x00u;
    check((static_cast<uint8_t>(SPSR) & SSCEN) == 0u,
          "software SSCEN clear lost");

    // ── 5) WCOL on overwrite-before-consume, shared clear pair ──────────────
    SPDR = 0x21u;  // completion pending
    SPDR = 0x22u;  // overwrite before consumption -> WCOL
    check((static_cast<uint8_t>(SPSR) & WCOL) != 0u,
          "WCOL must set on overwrite before the flag was consumed");
    check((static_cast<uint8_t>(SPSR) & SPISIF) != 0u,
          "SPISIF must stay set on overwrite");
    consume_flags();
    check((static_cast<uint8_t>(SPSR) & (SPISIF | WCOL)) == 0u,
          "SPSR->SPDR pair did not clear SPISIF+WCOL");

    // ── 6) Mock RX injection ────────────────────────────────────────────────
    cms8s_spi_set_rx_value(0x5Au);
    const uint8_t injected = vendor_spi_transmit(0x00u);
    check(injected == 0x5Au, "injected mock RX byte not returned");
    cms8s_spi_set_rx_value(0xFFu);

    // ── 7) Per-byte charge = ceil(8 / (Fsys/div)) ───────────────────────────
    const uint32_t step = wink_mcs51_get_microstep_us();
    SPCR = SPCR_DIV_8;  // Fspi = 12MHz/8 = 1.5MHz -> 6us
    {
        const uint64_t t0 = wink_mcs51_virtual_us();
        SPDR = 0x31u;
        check(cms8s_spi_last_charge_us() == 6u, "div8 charge != 6us");
        check(wink_mcs51_virtual_us() - t0 == 6u + step,
              "div8 virtual-time delta != charge + one microstep");
    }
    consume_flags();
    SPCR = SPCR_DIV_4;  // Fspi = 3MHz -> ceil(8/3) = 3us
    SPDR = 0x32u;
    check(cms8s_spi_last_charge_us() == 3u, "div4 charge != 3us");
    consume_flags();
    SPCR = SPCR_DIV_64;  // Fspi = 187.5kHz -> 43us
    SPDR = 0x33u;
    check(cms8s_spi_last_charge_us() == 43u, "div64 charge != 43us");
    consume_flags();
    SPCR = SPCR_DIV_8;

    // ── 8) SSCR.NSSO1 frame edges ───────────────────────────────────────────
    const uint32_t starts = cms8s_spi_frame_start_count();
    const uint32_t ends = cms8s_spi_frame_end_count();
    SSCR = static_cast<uint8_t>(SSCR & ~0x02u);  // SPI_M95256_Start
    SSCR = static_cast<uint8_t>(SSCR | 0x02u);   // SPI_M95256_Stop
    SSCR = static_cast<uint8_t>(SSCR | 0x02u);   // no edge (already high)
    check(cms8s_spi_frame_start_count() == starts + 1u,
          "NSSx 1->0 must count exactly one frame start");
    check(cms8s_spi_frame_end_count() == ends + 1u,
          "NSSx 0->1 must count exactly one frame end");

    // ── 9) Interrupt path: SPIIE + EA -> vector 22, SPIIF latched ───────────
    const uint32_t before = wink_mcs51_isr_dispatch_count(VECTOR_SPI);
    EIE2 = EIE2_SPIIE;
    IE = IE_EA;
    SPDR = 0x41u;
    check(wink_mcs51_isr_dispatch_count(VECTOR_SPI) == before + 1u,
          "SPIIE+EA must dispatch vector 22 exactly once");
    check(g_spi_isr_hits == 1u, "vector-22 ISR body must run once");
    check((static_cast<uint8_t>(EIF2) & EIF2_SPIIF) != 0u,
          "SPIIF must stay latched after vectoring (SW-cleared)");
    EIF2 = 0x00u;
    EIE2 = 0x00u;
    consume_flags();

    // ── 10) Host SPI bus fallback (Phase 2 prerequisite) ────────────────────
    {
        const uint8_t tx = 0x00u;
        uint8_t rx_buf = 0x00u;
        check(!js_pal_spi_transfer(0u, 0u, &tx, 1u, &rx_buf, 0u, 1000000u),
              "host js_pal_spi_transfer fallback must fail closed");
    }

    // ── 11) Reset restores the contract and re-installs hooks (S4-H2) ───────
    cms8s_spi_reset(ctx);
    check(static_cast<uint8_t>(SSCR) == 0x02u, "reset: SSCR != 0x02");
    check((static_cast<uint8_t>(SPSR) & (SPISIF | WCOL)) == 0u,
          "reset: status flags not clear");
    check(cms8s_spi_transfer_count() == 0u, "reset: transfer count != 0");
    check(cms8s_spi_frame_start_count() == 0u &&
              cms8s_spi_frame_end_count() == 0u,
          "reset: frame counters != 0");
    check(cms8s_spi_rx_value() == 0xFFu, "reset: mock RX != 0xFF");
    const uint8_t after_reset = vendor_spi_transmit(0x51u);
    check(after_reset == 0xFFu && cms8s_spi_transfer_count() == 1u,
          "reset did not re-install the SPDR hooks (S4-H2)");

    if (g_fails) {
        printf("[cms8s_spi] %d failure(s)\n", g_fails);
        return 1;
    }
    printf("[cms8s_spi] PASS: sync completion, two-step read-clear, SPSR "
           "write limits, WCOL, mock RX, Fsys/div charge, SSCR edges, "
           "vector-22 IRQ, reset semantics\n");
    return 0;
}
