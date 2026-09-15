// SPDX-License-Identifier: GPL-3.0-only
// Stage3-H7 decision-B contract test: ADC0832 custom board-channel net ids.
//
// The default-key path (32/33) is covered by the dio/e2e/iron suites. This
// TU proves the NEW stage3 surface: adc0832_device_attach() with explicit
// ch0/ch1_net_id, the adc0832_ch_key() mapping (incl. unbound fallback and
// the zero-clamp), and that the 0 us conversion pull follows the mapping.
// Order matters: the unbound-fallback section runs FIRST (device pool is
// BSS-zero in this fresh process; any attach flips net_bound permanently).
#include <stdint.h>
#include <stdio.h>

#include "adc0832.h"
#include "mcs51_adc.h"
#include "mcs51_context.h"
#include "mcs51_proxy.hpp"
#include "mcs51_trap.h"

namespace {

int g_fails = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        printf("[mcs51] FAIL: %s\n", what);
        ++g_fails;
    }
}

constexpr uint8_t P1_ADDR = 0x90;

// Minimal 3-wire read: CS fall, 3 config rises (start=1, SGL=1, ODD=ch),
// 8 falling-edge data reads. Re-attaches first (trap_reset wipes traps).
uint8_t adc_read_ch(const Adc0832Config* cfg, uint8_t channel) {
    mcs51_trap_reset();
    adc0832_device_attach(mcs51_get_context(), cfg);
    mcs51_get_context()->sfr_shadow[P1_ADDR] = 0xFFu;

    WinkSfr PORT(P1_ADDR);
    WinkSbit CS(PORT ^ 2), CLK(PORT ^ 1), DIO(PORT ^ 0);

    CS = 0;
    CLK = 0;
    DIO = 1;  CLK = 1; CLK = 0;                          // rise 1: start
    DIO = 1;  CLK = 1; CLK = 0;                          // rise 2: SGL
    DIO = channel ? 1u : 0u;
    CLK = 1;                                             // rise 3: latch
    DIO = 1;                                             // bus release

    uint8_t value = 0;
    for (int i = 0; i < 8; ++i) {
        CLK = 0;
        value = (uint8_t)((value << 1) | (static_cast<uint8_t>(DIO) & 1u));
        CLK = 1;
    }
    CS = 1;
    return value;
}

constexpr Adc0832Config kCustom = {
    1, 2, 1, 1, 1, 0, 1, 0,  // CS=P1.2 CLK=P1.1 DIO=P1.0
    40, 41,                  // custom board-channel net ids
};

constexpr Adc0832Config kZeroNets = {
    1, 2, 1, 1, 1, 0, 1, 0,
    0, 0,  // unspecified nets must clamp to defaults, not key 0 (P0.0)
};

void test_unbound_fallback() {
    Mcu51Context* ctx = mcs51_get_context();
    check(adc0832_ch_key(ctx, 0) == 32u, "unbound CH0 falls back to 32");
    check(adc0832_ch_key(ctx, 1) == 33u, "unbound CH1 falls back to 33");
}

void test_custom_nets_end_to_end() {
    Mcu51Context* ctx = mcs51_get_context();
    mcs51_adc_reset();
    // Inject DIRECTLY on the board keys (bypasses the device): if the
    // conversion pulled the hardcoded 32/33 instead of the mapping, the
    // reads below would come back 0x00.
    mcs51_adc_set_value(40u, 0xA5u);
    mcs51_adc_set_value(41u, 0x5Au);
    check(adc_read_ch(&kCustom, 0) == 0xA5u, "custom net 40 drives CH0");
    check(adc_read_ch(&kCustom, 1) == 0x5Au, "custom net 41 drives CH1");
    check(adc0832_ch_key(ctx, 0) == 40u, "ch_key maps CH0 to 40");
    check(adc0832_ch_key(ctx, 1) == 41u, "ch_key maps CH1 to 41");
}

void test_zero_nets_clamp() {
    Mcu51Context* ctx = mcs51_get_context();
    mcs51_adc_reset();
    mcs51_adc_set_value(32u, 0x5Au);
    check(adc_read_ch(&kZeroNets, 0) == 0x5Au, "zero net clamps CH0 to 32");
    check(adc0832_ch_key(ctx, 0) == 32u, "ch_key clamps CH0 zero to 32");
    check(adc0832_ch_key(ctx, 1) == 33u, "ch_key clamps CH1 zero to 33");
}

}  // namespace

extern "C" void wink_mcs51_user_main(void) {}
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

int main(void) {
    test_unbound_fallback();
    test_custom_nets_end_to_end();
    test_zero_nets_clamp();

    if (g_fails != 0) {
        printf("[mcs51] FAIL: ADC0832 attach net-id test had %d failures\n",
               g_fails);
        return 1;
    }
    printf("[mcs51] PASS: ADC0832 attach — custom nets 40/41 end-to-end, "
           "unbound fallback 32/33, zero-net clamp\n");
    return 0;
}
