// SPDX-License-Identifier: GPL-3.0-only
// A-02 ADC reference chain unit test (GAP-05): VSEL/Vrail scaling on the
// Pull track, LDO/mux readiness gates, DIV-sensitive charge_us.
#include <stdint.h>
#include <stdio.h>

#include "cms8s_adc.h"
#include "mcs51_adc.h"
#include "mcs51_context.h"
#include "mcs51_test_harness.h"
#include "mcs51_proxy.hpp"
#include "mcs51_xsfr.hpp"
#include "wink_mcs51_clock.h"

namespace {

constexpr uint8_t SFR_ADCON0 = 0xDF;
constexpr uint8_t SFR_ADCON1 = 0xDE;
constexpr uint8_t SFR_ADCCHS = 0xD9;
constexpr uint8_t SFR_ADRESL = 0xDC;
constexpr uint8_t SFR_ADRESH = 0xDD;

constexpr uint8_t ADCON0_ADFM = 0x40;
constexpr uint8_t ADCON0_ADGO = 0x02;
constexpr uint8_t ADCON1_ADEN = 0x80;

WinkSfr ADCON0(SFR_ADCON0);
WinkSfr ADCON1(SFR_ADCON1);
WinkSfr ADCCHS(SFR_ADCCHS);
WinkSfr ADRESL(SFR_ADRESL);
WinkSfr ADRESH(SFR_ADRESH);

int g_fails = 0;

void check(bool cond, const char *msg) {
    if (!cond) {
        printf("[adc-refchain] FAIL: %s\n", msg);
        ++g_fails;
    }
}

void convert(uint8_t ch, bool right, uint8_t adcks) {
    ADCCHS = ch;
    ADCON1 = static_cast<unsigned>(ADCON1_ADEN | ((adcks & 0x07u) << 4));
    ADCON0 = static_cast<unsigned>(right ? ADCON0_ADFM : 0u);
    ADCON0 = static_cast<unsigned>((uint8_t)ADCON0 | ADCON0_ADGO);
}

uint16_t result_right(void) {
    return static_cast<uint16_t>(((uint8_t)ADRESH << 8) | (uint8_t)ADRESL);
}

}  // namespace

extern "C" void wink_mcs51_user_main(void) {}
extern "C" void wink_mcs51_host_set_analog_norm(uint16_t pin, float norm);
extern "C" void wink_mcs51_host_analog_reset(void);
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

int main(void) {
    mcs51_test_register_family(MCS51_FAMILY_CMS8S78XX);
    mcs51_context_set_family(MCS51_FAMILY_CMS8S78XX);
    wink_mcs51_xdata_reset();
    mcs51_adc_reset();
    wink_mcs51_host_analog_reset();
    wink_mcs51_clock_reset();
    cms8s_adc_init(mcs51_get_context());
    cms8s_adc_model_reset(mcs51_get_context());

    // Baseline: LDO on + VSEL=3V, AN0 mux, Vrail=Vref=3.0V.
    mcs51_get_context()->xdata_shadow[0xF692u] = 0xE0u;
    mcs51_get_context()->xdata_shadow[0xF000u] = 0x01u;
    mcs51_adc_set_vrail_mv(3000u);

    // ── 1) VSEL parsing: VSEL=3V -> vref 3000 ─────────────────────────────
    {
        const uint32_t c0 = cms8s_adc_conversion_count();
        mcs51_adc_set_value(0, MCS51_ADC_RAIL_INJECT_NONE);
        // Stage1: AN0 pulls physical Pin 0 (was v1 synth key 32+0).
        wink_mcs51_host_set_analog_norm(0u, 0.5f);
        convert(0, true, 7u);
        check(mcs51_adc_get_vref_mv() == 3000u, "VSEL=3V must parse to 3000mV");
        check(cms8s_adc_conversion_count() == c0 + 1u, "conversion must run");
        // Pull track: 0.5*4095=2047.5->2048 (round half up), ratio 1.0.
        check(result_right() == 2048u, "0.5 norm @3V/3V must read 2048");
    }

    // ── 2) Vrail scaling: same ratio, Vrail=3.3V -> higher code ────────────
    {
        mcs51_adc_set_vrail_mv(3300u);
        wink_mcs51_host_set_analog_norm(0u, 0.5f);
        convert(0, true, 7u);
        // 2048 * 3300 / 3000 = 2252.8 -> 2252 (integer truncation).
        check(result_right() == 2252u, "0.5 norm @3V/3.3V must read 2252");
        mcs51_adc_set_vrail_mv(3000u);
    }

    // ── 3) LDO gate: LDOEN=0 blocks conversion + counts ───────────────────
    {
        mcs51_get_context()->xdata_shadow[0xF692u] = 0x60u;  // VSEL=3V, LDO off
        const uint32_t c0 = cms8s_adc_conversion_count();
        const uint32_t n0 = cms8s_adc_notready_total();
        convert(0, true, 7u);
        check(cms8s_adc_conversion_count() == c0, "LDO-off conversion must not run");
        check(cms8s_adc_notready_total() == n0 + 1u, "LDO-off must count");
        check(cms8s_adc_notready_count(WINK_MCS51_ADC_NOTREADY_LDO) == 1u,
              "LDO reason bucket must count");
        check(((uint8_t)ADCON0 & ADCON0_ADGO) == 0u, "ADGO must clear on gate");
        mcs51_get_context()->xdata_shadow[0xF692u] = 0xE0u;  // restore
        cms8s_adc_model_reset(mcs51_get_context());
        mcs51_get_context()->xdata_shadow[0xF692u] = 0xE0u;
        mcs51_get_context()->xdata_shadow[0xF000u] = 0x01u;
    }

    // ── 4) MUX gate: P00CFG != AN blocks conversion + counts ──────────────
    {
        mcs51_get_context()->xdata_shadow[0xF000u] = 0x00u;  // GPIO, not AN
        const uint32_t c0 = cms8s_adc_conversion_count();
        const uint32_t n0 = cms8s_adc_notready_total();
        convert(0, true, 7u);
        check(cms8s_adc_conversion_count() == c0, "mux-missing conversion must not run");
        check(cms8s_adc_notready_total() == n0 + 1u, "mux-missing must count");
        check(cms8s_adc_notready_count(WINK_MCS51_ADC_NOTREADY_MUX) == 1u,
              "MUX reason bucket must count");
        mcs51_get_context()->xdata_shadow[0xF000u] = 0x01u;  // restore
        cms8s_adc_model_reset(mcs51_get_context());
        mcs51_get_context()->xdata_shadow[0xF692u] = 0xE0u;
        mcs51_get_context()->xdata_shadow[0xF000u] = 0x01u;
    }

    // ── 5) DIV charge: DIV_256 conversion advances virtual time ────────────
    {
        wink_mcs51_clock_reset();
        cms8s_adc_model_reset(mcs51_get_context());
        mcs51_get_context()->xdata_shadow[0xF692u] = 0xE0u;
        mcs51_get_context()->xdata_shadow[0xF000u] = 0x01u;
        const uint64_t t0 = wink_mcs51_virtual_us();
        convert(0, true, 7u);  // DIV_256 @24MHz ≈ 170µs
        const uint64_t dt = wink_mcs51_virtual_us() - t0;
        check(dt >= 100u && dt <= 300u, "DIV_256 charge must be ~170us");
    }

    // ── 6) health_pot-equivalent config converts cleanly ───────────────────
    {
        cms8s_adc_model_reset(mcs51_get_context());
        mcs51_get_context()->xdata_shadow[0xF692u] = 0xE0u;
        mcs51_get_context()->xdata_shadow[0xF000u] = 0x01u;
        mcs51_adc_set_vrail_mv(3000u);
        wink_mcs51_host_set_analog_norm(0u, 0.25f);
        convert(0, true, 7u);
        check(cms8s_adc_notready_total() == 0u, "health_pot config must be clean");
        check(result_right() == 1024u, "0.25 norm must read 1024");
    }

    wink_mcs51_host_analog_reset();
    if (g_fails) {
        return 1;
    }
    printf("[adc-refchain] PASS: VSEL/Vrail scaling, LDO/mux gates, DIV charge\n");
    return 0;
}
