// SPDX-License-Identifier: Apache-2.0
// M5 tier-b harvest: compile and RUN the UNMODIFIED vendor StdDriver ADC API.
//
// The vendor source is docs/vendors/.../StdDriver/src/adc.c (reference-only,
// never committed). CMake runs the Keil-dialect cleanup pass on it (GBK->UTF-8
// + ISR rewrite) and compiles it as a C++ TU with the sandbox shim
// cms8s78xx.h on the include path, so the vendor functions operate on the
// WinkSfr/WinkXsfr proxies and drive the 0-cycle cms8s_adc model.
//
// This driver calls the vendor ADC_* API exactly as a Keil application would:
//   ADC_ConfigRunMode / ADC_EnableChannel / ADC_Start / ADC_GO /
//   ADC_GetADCResult / ADC_EnableInt / ADC_GetIntFlag / ADC_ClearIntFlag /
//   ADC_EnableLDO / ADC_ConfigADCVref / ADC_EnableLDOOutput / compare+trig.
// It asserts the model produces the injected 12-bit codes through the vendor
// read formulas, the EOC interrupt vector 19 fires under the vendor enable
// calls, and the XSFR LDO calls land in the legal window.
#include <stdint.h>
#include <stdio.h>

// Vendor StdDriver public API (cleaned StdDriver/inc/adc.h in the build tree).
#include "adc.h"

#include "absacc.h"
#include "cms8s_adc.h"
#include "mcs51_adc.h"
#include "mcs51_proxy.hpp"
#include "mcs51_xsfr.hpp"
#include "wink_mcs51_isr.h"

// REGX52.H (pulled in via adc.h -> cms8s78xx.h -> REG_CMS8S.H) #defines main to
// wink_mcs51_user_main for Keil super-loop sources. This is a host test with its
// own main(), so drop the remap and provide the empty fiber entry the bridge
// references.
#undef main
extern "C" void wink_mcs51_user_main(void) {}

namespace {

int g_fails = 0;

void check(bool cond, const char *msg) {
    if (!cond) {
        printf("[mcs51] FAIL: %s\n", msg);
        ++g_fails;
    }
}

uint32_t g_isr_hits = 0;

// One vendor-style polled conversion: enable channel, start, GO, poll busy.
uint16_t vendor_convert(uint8_t ch) {
    ADC_EnableChannel(ch);
    ADC_Start();
    ADC_GO();
    while (ADC_IS_BUSY) {
        // 0-cycle model: ADGO self-clears inside the ADC_GO() write, so this
        // loop body never runs on real silicon semantics.
    }
    return ADC_GetADCResult();
}

}  // namespace

WINK_ISR(19) {
    ++g_isr_hits;
    // Vendor ISR idiom: clear the latched flag by software.
    ADC_ClearIntFlag();
}

extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

int main(void) {
    wink_mcs51_isr_enable();
    wink_mcs51_xdata_reset();
    mcs51_adc_reset();
    cms8s_adc_init();

    // ── 1) Right-justify mode through the vendor config API ─────────────────
    // ADC_ConfigRunMode(div, ADC_RESULT_RIGHT) sets ADFM; ADCKS is accepted.
    ADC_ConfigRunMode(ADC_CLK_DIV_16, ADC_RESULT_RIGHT);

    mcs51_adc_set_value(0, 0x0ABCu);
    uint16_t r = vendor_convert(ADC_CH_0);
    check(r == 0x0ABCu, "vendor API AN0 right: want 0xABC");
    check(cms8s_adc_conversion_count() == 1u, "vendor API conversion count != 1");
    check(cms8s_adc_last_channel() == 0u, "vendor API last channel != AN0");

    mcs51_adc_set_value(1, 0x0801u);
    r = vendor_convert(ADC_CH_1);
    check(r == 0x0801u, "vendor API AN1 right: want 0x801");

    mcs51_adc_set_value(25, 0x0FFFu);
    r = vendor_convert(ADC_CH_25);
    check(r == 0x0FFFu, "vendor API AN25 right: want 0xFFF");
    check(cms8s_adc_last_channel() == 25u, "vendor API last channel != AN25");

    // ── 2) Left-justify mode: vendor GetADCResult picks the other formula ───
    ADC_ConfigRunMode(ADC_CLK_DIV_8, ADC_RESULT_LEFT);
    mcs51_adc_set_value(2, 0x0ABCu);
    r = vendor_convert(ADC_CH_2);
    check(r == 0x0ABCu, "vendor API AN2 left: want 0xABC (recombined)");

    // ── 3) Stop gate: ADC_Stop clears ADEN, conversion must not run ────────
    const uint32_t count_before = cms8s_adc_conversion_count();
    ADC_Stop();
    ADC_EnableChannel(ADC_CH_0);
    ADC_GO();
    check(cms8s_adc_conversion_count() == count_before,
          "vendor ADC_Stop did not gate conversion");
    ADC_Start();  // re-enable for the rest

    // ── 4) EOC interrupt through the vendor int API (vector 19) ────────────
    ADC_EnableInt();
    IE = 0x80;  // EA (REGX52.H sfr)
    const uint32_t disp_before = wink_mcs51_isr_dispatch_count(19);
    mcs51_adc_set_value(3, 0x555u);
    r = vendor_convert(ADC_CH_3);
    check(r == 0x0555u, "vendor API AN3 right: want 0x555");
    check(wink_mcs51_isr_dispatch_count(19) == disp_before + 1u,
          "vendor ADC_EnableInt did not dispatch vector 19");
    check(g_isr_hits == 1u, "vendor vector-19 ISR body not run once");
    // ISR called ADC_ClearIntFlag(): flag now reads 0 via the vendor getter.
    check(ADC_GetIntFlag() == 0u,
          "vendor ADC_GetIntFlag set after ADC_ClearIntFlag in ISR");
    ADC_DisableInt();

    // ── 5) XSFR LDO through the vendor API (WinkXsfr window, no OOB) ───────
    const uint32_t oob_before = wink_mcs51_xdata_oob_count();
    ADC_EnableLDO();                       // ADCLDO |= LDOEN (0x80)
    check((uint8_t)ADCLDO == 0x80u, "vendor ADC_EnableLDO: ADCLDO want 0x80");
    ADC_ConfigADCVref(ADC_VREF_3V);        // VSEL field (ignored by v1 model)
    ADC_EnableLDOOutput();                 // ADCLDO |= OUTEN (0x10)
    // LDOEN(0x80) | VSEL_3V(0x3<<5=0x60) | OUTEN(0x10) = 0xF0
    check((uint8_t)ADCLDO == 0xF0u, "vendor LDO config: ADCLDO want 0xF0");
    ADC_DisableLDOOutput();
    check(((uint8_t)ADCLDO & 0x10u) == 0u,
          "vendor ADC_DisableLDOOutput did not clear OUTEN");
    ADC_DisableLDO();
    check(((uint8_t)ADCLDO & 0x80u) == 0u,
          "vendor ADC_DisableLDO did not clear LDOEN");
    check(wink_mcs51_xdata_oob_count() == oob_before,
          "vendor XSFR LDO access counted as OOB");

    // ── 6) Compare / trigger / AN63 config calls: smoke (accepted, no fault)
    ADC_ConfigCompareValue(0x800u);        // ADCMPL/ADCMPH
    ADC_ConfigADCCMPOutput(ADC_ADRES_LESS_THAN_ADCMP);
    (void)ADC_GetCmpResult();
    ADC_EnableHardwareTrig();
    ADC_ConfigHardwareTrig(ADC_TG_PWM0, ADC_TG_RISING);
    ADC_SetTrigDelayTime(1000u);
    ADC_DisableHardwareTrig();  // vendor spelling: "Trig" (no "-ger")
    ADC_ConfigAN63(ADC_CH_63_BGR);
    mcs51_adc_set_value(0x3F, 0x0000u);    // AN63 v1 returns 0
    r = vendor_convert(ADC_CH_63);
    check(r == 0x0000u, "vendor API AN63 internal: v1 want 0");

    if (g_fails) {
        return 1;
    }
    printf("[mcs51] PASS: vendor CMS8S78xx StdDriver adc.c runs unmodified in "
           "the sandbox — ADC_* config/start/GO/result, vector-19 EOC int, "
           "XSFR LDO, compare/trig/AN63 smoke via the 0-cycle model\n");
    return 0;
}
