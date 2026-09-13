// SPDX-License-Identifier: Apache-2.0
// Unit tests for CMS8S78xx on-chip Analog Comparator (ACMP0 & ACMP1) peripheral model.
#include <stdint.h>
#include <stdio.h>

#include "cms8s_acmp.h"
#include "cms8s78xx.h"
#include "mcs51_context.h"
#include "mcs51_test_harness.h"
#include "wink_mcs51_gpio.h"
#include "wink_mcs51_isr.h"

#undef main
#undef printf
extern "C" void wink_mcs51_user_main(void) {}
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

extern "C" {
uint32_t wink_mcs51_host_gpio_notify_count(void);
void wink_mcs51_host_gpio_notify_reset(void);
uint16_t wink_mcs51_host_gpio_notify_pin(uint32_t i);
uint8_t wink_mcs51_host_gpio_notify_level(uint32_t i);
}

namespace {

int g_fails = 0;

void check(bool cond, const char *msg) {
    if (!cond) {
        printf("[mcs51_acmp] FAIL: %s\n", msg);
        ++g_fails;
    }
}

uint8_t get_last_pin_level(uint16_t pin) {
    uint32_t n = wink_mcs51_host_gpio_notify_count();
    for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
        if (wink_mcs51_host_gpio_notify_pin(static_cast<uint32_t>(i)) == pin) {
            return wink_mcs51_host_gpio_notify_level(static_cast<uint32_t>(i));
        }
    }
    return 0xFFu;
}

}  // namespace

int main(void) {
    printf("[mcs51_acmp] Starting CMS8S78xx ACMP unit tests...\n");

    mcs51_test_register_family(MCS51_FAMILY_CMS8S78XX);
    mcs51_context_set_family(MCS51_FAMILY_CMS8S78XX);
    Mcu51Context* ctx = mcs51_get_context();
    mcs51_context_reset(ctx);
    wink_mcs51_isr_enable();
    wink_mcs51_xdata_reset();
    wink_mcs51_host_gpio_notify_reset();

    cms8s_acmp_init(ctx);

    // ── 1) Test ACMP0: P1.1 (pin 9) vs Bandgap 1.2V (norm 0.24) ──
    ACMP_ConfigPositive(ACMP0, ACMP_POSSEL_P0);
    ACMP_ConfigNegative(ACMP0, ACMP_NEGSEL_BG, 0);
    ACMP_DisableReverseOutput(ACMP0);
    ACMP_EnableInt(ACMP0);
    GPIO_SET_MUX_MODE(P11CFG, GPIO_P11_MUX_C0P0);
    GPIO_SET_MUX_MODE(P10CFG, GPIO_P10_MUX_C0O);
    ACMP_Start(ACMP0);

    // Pin 9 injected = 0.10 (< 0.24) -> 0.10 * 4095 = 409
    ctx->adc_inject_flag[9] = 1u;
    ctx->adc_injected[9] = 409u;
    cms8s_acmp_poll(ctx);
    check(ACMP_GetResult(ACMP0) == 0u, "ACMP0 result should be 0 below threshold");
    check(ACMP_GetIntFlag(ACMP0) == 0u, "ACMP0 flag should not be set");

    // Pin 9 injected = 0.50 (> 0.24) -> 0.50 * 4095 = 2047
    ctx->adc_injected[9] = 2047u;
    cms8s_acmp_poll(ctx);
    check(ACMP_GetResult(ACMP0) == 1u, "ACMP0 result should be 1 above threshold");
    check(ACMP_GetIntFlag(ACMP0) != 0u, "ACMP0 flag should be set on rising edge");
    check(get_last_pin_level(8u) == 1u, "P1.0 (C0O) should be driven high");

    // Clear interrupt flag
    ACMP_ClearIntFlag(ACMP0);
    check(ACMP_GetIntFlag(ACMP0) == 0u, "ACMP0 flag should clear");

    // Pin 9 drops below threshold -> output 0, no rising edge interrupt
    ctx->adc_injected[9] = 409u;
    cms8s_acmp_poll(ctx);
    check(ACMP_GetResult(ACMP0) == 0u, "ACMP0 result should drop to 0");
    check(ACMP_GetIntFlag(ACMP0) == 0u, "ACMP0 flag should remain 0 on falling edge");
    check(get_last_pin_level(8u) == 0u, "P1.0 (C0O) should be driven low");

    // Stop ACMP0
    ACMP_Stop(ACMP0);
    cms8s_acmp_poll(ctx);
    check(ACMP_GetResult(ACMP0) == 0u, "ACMP0 result should be 0 when stopped");

    // ── 2) Test ACMP1: P2.1 (pin 17) vs 20% Bandgap (0.24V * 0.20 = 0.048V, norm 0.048) ──
    ACMP_ConfigPositive(ACMP1, ACMP_POSSEL_P0);
    ACMP_ConfigNegative(ACMP1, ACMP_NEGSEL_VREF_BG, Vref_K_2000_10000);
    ACMP_DisableReverseOutput(ACMP1);
    ACMP_EnableInt(ACMP1);
    GPIO_SET_MUX_MODE(P21CFG, GPIO_P21_MUX_C1P0);
    GPIO_SET_MUX_MODE(P24CFG, GPIO_P24_MUX_C1O);
    ACMP_Start(ACMP1);

    // Pin 17 injected = 0.02 (< 0.048) -> 0.02 * 4095 = 82
    ctx->adc_inject_flag[17] = 1u;
    ctx->adc_injected[17] = 82u;
    cms8s_acmp_poll(ctx);
    check(ACMP_GetResult(ACMP1) == 0u, "ACMP1 result should be 0 below threshold");
    check(ACMP_GetIntFlag(ACMP1) == 0u, "ACMP1 flag should not be set");

    // Pin 17 injected = 0.10 (> 0.048) -> 0.10 * 4095 = 409
    ctx->adc_injected[17] = 409u;
    cms8s_acmp_poll(ctx);
    check(ACMP_GetResult(ACMP1) == 1u, "ACMP1 result should be 1 above threshold");
    check(ACMP_GetIntFlag(ACMP1) != 0u, "ACMP1 flag should be set on rising edge");
    check(get_last_pin_level(20u) == 1u, "P2.4 (C1O) should be driven high");

    // Clear interrupt flag
    ACMP_ClearIntFlag(ACMP1);
    check(ACMP_GetIntFlag(ACMP1) == 0u, "ACMP1 flag should clear");

    // Pin 17 drops below threshold
    ctx->adc_injected[17] = 82u;
    cms8s_acmp_poll(ctx);
    check(ACMP_GetResult(ACMP1) == 0u, "ACMP1 result should drop to 0");
    check(ACMP_GetIntFlag(ACMP1) == 0u, "ACMP1 flag should remain 0 on falling edge");
    check(get_last_pin_level(20u) == 0u, "P2.4 (C1O) should be driven low");

    // Pin 17 rises above threshold again -> second trigger
    ctx->adc_injected[17] = 820u; // 0.20
    cms8s_acmp_poll(ctx);
    check(ACMP_GetResult(ACMP1) == 1u, "ACMP1 result should be 1 on second crossing");
    check(ACMP_GetIntFlag(ACMP1) != 0u, "ACMP1 flag should be set on second rising edge");
    check(get_last_pin_level(20u) == 1u, "P2.4 (C1O) should be driven high again");

    ACMP_Stop(ACMP1);

    if (g_fails == 0) {
        printf("[mcs51_acmp] All ACMP tests passed successfully!\n");
        return 0;
    }
    printf("[mcs51_acmp] FAILED: %d checks failed\n", g_fails);
    return 1;
}
