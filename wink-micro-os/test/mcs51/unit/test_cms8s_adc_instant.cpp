// SPDX-License-Identifier: Apache-2.0
// M5 CMS8S78xx on-chip ADC unit test: the 0-cycle instant-conversion model.
//
// Drives the model through the REAL proxy path (WinkSfr stores the shadow
// first, then fires the ADCON0 write hook registered by cms8s_adc_init):
//   * ADGO self-clears inside the triggering write (poll exits first read);
//   * right/left result packing matches the vendor ADC_GetADCResult formulas;
//   * end-of-conversion: EIE2.ADCIE latches EIF2.ADCIF, EA gates vector 19;
//   * ADEN=0 gates conversion; channel 25 passes through; AN63 (0x3F) reads 0;
//   * XSFR proxy (WinkXsfr) reaches the 0xF000 window with no OOB, and an
//     access to 0xE000 still traps (drop write / 0xFF read / OOB count).
#include <stdint.h>
#include <stdio.h>

#include "absacc.h"
#include "cms8s_adc.h"
#include "mcs51_adc.h"
#include "mcs51_proxy.hpp"
#include "mcs51_xsfr.hpp"
#include "wink_mcs51_isr.h"

namespace {

constexpr uint8_t SFR_ADCON0 = 0xDF;
constexpr uint8_t SFR_ADCON1 = 0xDE;
constexpr uint8_t SFR_ADCCHS = 0xD9;
constexpr uint8_t SFR_ADRESL = 0xDC;
constexpr uint8_t SFR_ADRESH = 0xDD;
constexpr uint8_t SFR_EIE2   = 0xAA;
constexpr uint8_t SFR_EIF2   = 0xB2;
constexpr uint8_t SFR_IE     = 0xA8;

constexpr uint8_t ADCON0_ADFM = 0x40;
constexpr uint8_t ADCON0_ADGO = 0x02;
constexpr uint8_t ADCON1_ADEN = 0x80;
constexpr uint8_t EIE2_ADCIE  = 0x10;
constexpr uint8_t EIF2_ADCIF  = 0x10;
constexpr uint8_t IE_EA       = 0x80;
constexpr uint8_t VECTOR_ADC  = 19u;

// SFR proxies bound to the real vendor addresses (shadow-first + hook path).
WinkSfr ADCON0(SFR_ADCON0);
WinkSfr ADCON1(SFR_ADCON1);
WinkSfr ADCCHS(SFR_ADCCHS);
WinkSfr ADRESL(SFR_ADRESL);
WinkSfr ADRESH(SFR_ADRESH);
WinkSfr EIE2(SFR_EIE2);
WinkSfr EIF2(SFR_EIF2);
WinkSfr IE(SFR_IE);

uint32_t g_adc_isr_hits = 0;

int g_fails = 0;

void check(bool cond, const char *msg) {
    if (!cond) {
        printf("[mcs51] FAIL: %s\n", msg);
        ++g_fails;
    }
}

// One polled conversion: select channel, justify, write ADGO.
void convert(uint8_t ch, bool right) {
    ADCCHS = ch;
    ADCON1 = ADCON1_ADEN;
    ADCON0 = static_cast<unsigned>(right ? ADCON0_ADFM : 0u);
    ADCON0 = static_cast<unsigned>(
        (uint8_t)ADCON0 | ADCON0_ADGO);  // ADC_GO() idiom (RMW)
}

}  // namespace

WINK_ISR(19) {
    ++g_adc_isr_hits;
}

// The bridge TU references the user entry; this test drives the model
// directly, so an empty definition closes the link.
extern "C" void wink_mcs51_user_main(void) {}

extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

int main(void) {
    wink_mcs51_isr_enable();       // open the execution-phase dispatch gate
    wink_mcs51_xdata_reset();      // clean XDATA shadow + OOB counters
    mcs51_adc_reset();             // clear injection rail
    cms8s_adc_init();              // register ADCON0 write hook, zero counters

    // ── 1) 0-cycle passthrough: ADGO self-clears inside the write ───────────
    mcs51_adc_set_value(0, 0x0ABCu);
    convert(0, true);
    check(((uint8_t)ADCON0 & ADCON0_ADGO) == 0u,
          "ADGO still set after the triggering write (must self-clear)");
    check(cms8s_adc_conversion_count() == 1u, "conversion count != 1");

    // ── 2) Right-justify packing: result = (ADRESH<<8)|ADRESL ───────────────
    check((uint8_t)ADRESH == 0x0Au && (uint8_t)ADRESL == 0xBCu,
          "right 0xABC -> ADRESH/ADRESL want 0x0A/0xBC");

    mcs51_adc_set_value(0, 0x0FFFu);
    convert(0, true);
    check((uint8_t)ADRESH == 0x0Fu && (uint8_t)ADRESL == 0xFFu,
          "right 0xFFF -> want 0x0F/0xFF");

    mcs51_adc_set_value(0, 0x0000u);
    convert(0, true);
    check((uint8_t)ADRESH == 0x00u && (uint8_t)ADRESL == 0x00u,
          "right 0x000 -> want 0x00/0x00");

    mcs51_adc_set_value(1, 0x0801u);
    convert(1, true);
    check((uint8_t)ADRESH == 0x08u && (uint8_t)ADRESL == 0x01u,
          "right 0x801 -> want 0x08/0x01");

    // ── 3) Left-justify packing: result = (ADRESH<<4)|(ADRESL>>4) ───────────
    mcs51_adc_set_value(1, 0x0ABCu);
    convert(1, false);
    check((uint8_t)ADRESH == 0xABu && (uint8_t)ADRESL == 0xC0u,
          "left 0xABC -> want 0xAB/0xC0");

    mcs51_adc_set_value(1, 0x0801u);
    convert(1, false);
    check((uint8_t)ADRESH == 0x80u && (uint8_t)ADRESL == 0x10u,
          "left 0x801 -> want 0x80/0x10");

    // ── 4) Interrupt: EIE2.ADCIE + IE.EA -> vector 19 exactly once, flag held
    const uint32_t before = wink_mcs51_isr_dispatch_count(VECTOR_ADC);
    EIE2 = EIE2_ADCIE;
    IE   = IE_EA;
    mcs51_adc_set_value(0, 0x777u);
    convert(0, true);
    check(wink_mcs51_isr_dispatch_count(VECTOR_ADC) == before + 1u,
          "vector 19 not dispatched exactly once with ADCIE+EA");
    check(g_adc_isr_hits == 1u, "vector-19 ISR body not run once");
    check(((uint8_t)EIF2 & EIF2_ADCIF) != 0u,
          "ADCIF must stay latched after vectoring (SW-cleared, like TI)");

    // ── 5) ADCIE=0: no flag, no dispatch ────────────────────────────────────
    EIE2 = 0u;
    EIF2 = 0u;  // software clear (vendor EIF2 = 0xFF & ~ADCIF)
    mcs51_adc_set_value(0, 0x778u);
    convert(0, true);
    check(wink_mcs51_isr_dispatch_count(VECTOR_ADC) == before + 1u,
          "vector 19 dispatched with ADCIE clear");
    check(((uint8_t)EIF2 & EIF2_ADCIF) == 0u,
          "ADCIF set with ADCIE clear");

    // ── 6) ADCIE=1, EA=0: flag latches, no dispatch ─────────────────────────
    EIE2 = EIE2_ADCIE;
    EIF2 = 0u;
    IE   = 0u;
    mcs51_adc_set_value(0, 0x779u);
    convert(0, true);
    check(wink_mcs51_isr_dispatch_count(VECTOR_ADC) == before + 1u,
          "vector 19 dispatched with EA clear");
    check(((uint8_t)EIF2 & EIF2_ADCIF) != 0u,
          "ADCIF not latched with ADCIE set / EA clear");

    // ── 7) ADEN=0 gates conversion (ADGO write ignored, no result update) ───
    const uint32_t count_now = cms8s_adc_conversion_count();
    ADCCHS = 0u;
    ADCON1 = 0u;            // module disabled
    ADRESH = 0u; ADRESL = 0u;
    ADCON0 = ADCON0_ADFM | ADCON0_ADGO;
    check(cms8s_adc_conversion_count() == count_now,
          "conversion ran with ADEN clear");

    // ── 8) Channel 25 (AN25 = P3.1) passes through the 32-entry rail ────────
    mcs51_adc_set_value(25, 0x0FFFu);
    convert(25, true);
    check((uint8_t)ADRESH == 0x0Fu && (uint8_t)ADRESL == 0xFFu,
          "AN25 right 0xFFF -> want 0x0F/0xFF");
    check(cms8s_adc_last_channel() == 25u, "last channel != 25");

    // ── 9) AN63 internal mux (0x3F): v1 returns 0, no fault ─────────────────
    convert(0x3Fu, true);
    check((uint8_t)ADRESH == 0x00u && (uint8_t)ADRESL == 0x00u,
          "AN63 internal conversion should read 0 in v1");

    // ── 10) XSFR proxy: ADCLDO @ 0xF692 lives in the legal window ───────────
    WinkXsfr adcldo(0xF692u);
    const uint32_t oob_before = wink_mcs51_xdata_oob_count();
    adcldo = 0x80u;                       // LDOEN
    check((uint8_t)adcldo == 0x80u, "ADCLDO store/readback mismatch");
    adcldo = static_cast<unsigned>((uint8_t)adcldo | 0x10u);  // RMW (OUTEN)
    check((uint8_t)adcldo == 0x90u, "ADCLDO RMW want 0x90");
    check(wink_mcs51_xdata_shadow[0xF692u] == 0x90u,
          "ADCLDO did not land in the XSFR window shadow");
    check(wink_mcs51_xdata_oob_count() == oob_before,
          "in-window XSFR access counted as OOB");

    // ── 11) XSFR outside the window (0xE000) still traps: drop/0xFF/count ───
    WinkXsfr bad(0xE000u);
    bad = 0x55u;
    const uint8_t readback = static_cast<uint8_t>(bad);
    check(readback == 0xFFu, "OOB XSFR read should return 0xFF");
    check(wink_mcs51_xdata_shadow[0xE000u] == 0x00u,
          "OOB XSFR write must be dropped");
    check(wink_mcs51_xdata_oob_count() >= oob_before + 2u,
          "OOB XSFR access not counted (want >= 2: write + read)");

    if (g_fails) {
        return 1;
    }
    printf("[mcs51] PASS: CMS8S78xx ADC 0-cycle model — ADGO self-clear, "
           "right/left packing, ADCIE/EA vector-19 gating, ADEN gate, "
           "AN25/AN63 channels, XSFR window + OOB trap\n");
    return 0;
}
