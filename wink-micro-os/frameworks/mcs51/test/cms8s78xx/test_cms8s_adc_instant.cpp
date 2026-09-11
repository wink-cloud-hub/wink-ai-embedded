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
#include "mcs51_context.h"
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
extern "C" void wink_mcs51_host_set_ext_pin(uint16_t pin, uint8_t state);
extern "C" void wink_mcs51_host_set_analog_norm(uint16_t pin, float norm);
extern "C" void wink_mcs51_host_analog_reset(void);

extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

int main(void) {
    // M1: this test drives CMS8S-only silicon (ADCON0/XSFR window) — select
    // the family explicitly (the compat lib defaults to classic).
    mcs51_context_set_family(MCS51_FAMILY_CMS8S78XX);
    wink_mcs51_isr_enable();       // open the execution-phase dispatch gate
    wink_mcs51_xdata_reset();      // clean XDATA shadow + OOB counters
    mcs51_adc_reset();             // clear injection rail
    cms8s_adc_init(mcs51_get_context());  // register ADCON0 write hook, zero counters
    // A-02 gates (GAP-05): LDO on + VSEL=3V, analog mux for exercised channels.
    mcs51_get_context()->xdata_shadow[0xF692u] = 0xE0u;  // ADCLDO LDOEN+VSEL_3V
    mcs51_get_context()->xdata_shadow[0xF000u] = 0x01u;  // P00CFG=AN0 (ch0)
    mcs51_get_context()->xdata_shadow[0xF001u] = 0x01u;  // P01CFG=AN1 (ch1)
    mcs51_get_context()->xdata_shadow[0xF033u] = 0x01u;  // P33CFG=AN25 (ch25)

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

    // ── 8) Channel 25 (AN25 = P3.3 = Pin 27) passes through the 64-entry rail ─
    mcs51_adc_set_value(27, 0x0FFFu);
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
    check(mcs51_get_context()->xdata_shadow[0xF692u] == 0x90u,
          "ADCLDO did not land in the XSFR window shadow");
    check(wink_mcs51_xdata_oob_count() == oob_before,
          "in-window XSFR access counted as OOB");

    // ── 11) XSFR outside the window (0xE000) still traps: drop/0xFF/count ───
    WinkXsfr bad(0xE000u);
    bad = 0x55u;
    const uint8_t readback = static_cast<uint8_t>(bad);
    check(readback == 0xFFu, "OOB XSFR read should return 0xFF");
    check(mcs51_get_context()->xdata_shadow[0xE000u] == 0x00u,
          "OOB XSFR write must be dropped");
    check(wink_mcs51_xdata_oob_count() >= oob_before + 2u,
          "OOB XSFR access not counted (want >= 2: write + read)");

    // ── 12) Hardware trigger: ADCON2 ADCEX + ADET falling edge ────────────
    WinkSfr  ADCON2(0xE9u);
    WinkXsfr ps_adet(0xF0CCu);

    cms8s_adc_model_reset(mcs51_get_context());
    ADCON1 = ADCON1_ADEN;
    ADCON0 = 0u;  // left justify, ADGO=0
    ADCCHS = 0u;  // channel 0 (AN0)
    ADCON2 = 0x80u | (0x03u << 4) | (0x00u << 2);  // ADCEX=1, ADTGS=ADET (3), ADEGS=FALLING (0)
    ps_adet = 0x05u;                                // P0.5

    EIE2 = EIE2_ADCIE;
    IE   = IE_EA;
    EIF2 = 0u;

    mcs51_adc_set_value(0, 0x0567u);
    const uint32_t count_hw_start = cms8s_adc_conversion_count();
    const uint32_t isr_hw_start   = g_adc_isr_hits;

    // Pin initially high (idle with pullup)
    wink_mcs51_host_set_ext_pin(5, 1);
    cms8s_adc_poll(mcs51_get_context());
    check(cms8s_adc_conversion_count() == count_hw_start,
          "hardware trigger fired on baseline sample");

    // Falling edge: pin goes low -> conversion triggered
    wink_mcs51_host_set_ext_pin(5, 0);
    cms8s_adc_poll(mcs51_get_context());
    check(cms8s_adc_conversion_count() == count_hw_start + 1u,
          "hardware trigger did not fire on falling edge");
    check(g_adc_isr_hits == isr_hw_start + 1u,
          "vector 19 not dispatched on hardware trigger EOC");
    check((uint8_t)ADRESH == (uint8_t)((0x567u >> 4) & 0xFFu),
          "hardware trigger result ADRESH mismatch");

    // Pin held low -> no duplicate trigger
    cms8s_adc_poll(mcs51_get_context());
    check(cms8s_adc_conversion_count() == count_hw_start + 1u,
          "hardware trigger re-fired while pin held low");

    // Rising edge -> should not trigger in falling-edge mode
    wink_mcs51_host_set_ext_pin(5, 1);
    cms8s_adc_poll(mcs51_get_context());
    check(cms8s_adc_conversion_count() == count_hw_start + 1u,
          "hardware trigger fired on rising edge in falling-only mode");

    // Second falling edge -> triggers second conversion
    wink_mcs51_host_set_ext_pin(5, 0);
    cms8s_adc_poll(mcs51_get_context());
    check(cms8s_adc_conversion_count() == count_hw_start + 2u,
          "hardware trigger did not fire on second falling edge");
    check(g_adc_isr_hits == isr_hw_start + 2u,
          "vector 19 not dispatched on second hardware trigger EOC");

    // ── 13) Hardware trigger: rising edge mode (ADC_TG_RISING) ─────────────
    ADCON2 = 0x80u | (0x03u << 4) | (0x01u << 2);  // ADCEX=1, ADTGS=ADET (3), ADEGS=RISING (1)
    const uint32_t count_rising_start = cms8s_adc_conversion_count();
    const uint32_t isr_rising_start   = g_adc_isr_hits;

    // Pin goes low (falling edge) -> should NOT trigger in rising-only mode
    wink_mcs51_host_set_ext_pin(5, 0);
    cms8s_adc_poll(mcs51_get_context());
    check(cms8s_adc_conversion_count() == count_rising_start,
          "hardware trigger fired on falling edge in rising-only mode");

    // Pin goes high (rising edge) -> triggers conversion
    wink_mcs51_host_set_ext_pin(5, 1);
    cms8s_adc_poll(mcs51_get_context());
    check(cms8s_adc_conversion_count() == count_rising_start + 1u,
          "hardware trigger did not fire on rising edge");
    check(g_adc_isr_hits == isr_rising_start + 1u,
          "vector 19 not dispatched on rising edge trigger EOC");

    // ── 14) Stage1 dual-read compat (deleted stage7) ──────────────────────
    // Old pull-track drivers feed the synth key 32+ch for AN0 while the
    // physical Pin 0 pulls 0.0: expect redirect value + count. Then: synth
    // 0.0 must NOT redirect (true 0V protection); an injected pin key must
    // win outright without rerouting (injection rail unaffected).
    {
        cms8s_adc_model_reset(mcs51_get_context());  // redirect count -> 0
        wink_mcs51_host_analog_reset();
        mcs51_adc_reset();
        mcs51_get_context()->xdata_shadow[0xF692u] = 0xE0u;  // LDOEN+VSEL_3V
        mcs51_get_context()->xdata_shadow[0xF000u] = 0x01u;  // P00CFG=AN0
        mcs51_adc_set_vrail_mv(3000u);
        // (a) synth-key pull redirects: 0.5 norm -> 2048 right-justified.
        wink_mcs51_host_set_analog_norm(32u, 0.5f);  // v1 misuse: AN0 via 32+0
        convert(0, true);
        check((uint8_t)ADRESH == 0x08u && (uint8_t)ADRESL == 0x00u,
              "synth-key pull must redirect to 2048 (0x0800)");
        check(cms8s_adc_synth_redirect_count == 1u,
              "synth redirect must count exactly once");
        // (b) true 0V: synth pulls 0.0 too -> no redirect, raw stays 0.
        wink_mcs51_host_analog_reset();
        convert(0, true);
        check((uint8_t)ADRESH == 0x00u && (uint8_t)ADRESL == 0x00u,
              "true 0V short must report 0, never redirect");
        check(cms8s_adc_synth_redirect_count == 1u,
              "true 0V must not count a redirect");
        // (c) switch OFF kills the redirect even with synth driven.
        cms8s_adc_dual_read_synth = false;
        wink_mcs51_host_set_analog_norm(32u, 0.5f);
        convert(0, true);
        check((uint8_t)ADRESH == 0x00u && (uint8_t)ADRESL == 0x00u,
              "dual-read OFF must not redirect");
        check(cms8s_adc_synth_redirect_count == 1u,
              "dual-read OFF must not count");
        cms8s_adc_dual_read_synth = true;
        // (d) injected pin key wins without rerouting.
        wink_mcs51_host_analog_reset();
        mcs51_adc_set_value(0, 0x0123u);
        convert(0, true);
        check((uint8_t)ADRESH == 0x01u && (uint8_t)ADRESL == 0x23u,
              "injected pin key must win outright (0x123)");
        check(cms8s_adc_synth_redirect_count == 1u,
              "injected pin key must not count a redirect");
    }

    if (g_fails) {
        return 1;
    }
    printf("[mcs51] PASS: CMS8S78xx ADC 0-cycle model — ADGO self-clear, "
           "right/left packing, ADCIE/EA vector-19 gating, ADEN gate, "
           "AN25/AN63 channels, XSFR window + OOB trap, ADET hardware trigger (falling & rising), "
           "Stage1 synth-key dual-read compat\n");
    return 0;
}
