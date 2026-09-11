// SPDX-License-Identifier: Apache-2.0
// Unit tests for CMS8S78xx on-chip Buzzer peripheral model.
#include <stdint.h>
#include <stdio.h>

#include "cms8s_buzzer.h"
#include "cms8s78xx.h"
#include "mcs51_context.h"
#include "mcs51_test_harness.h"
#include "wink_mcs51_gpio.h"
#include "wink_mcs51_clock.h"

#undef main
#undef printf
extern "C" void wink_mcs51_user_main(void) {}
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}
// (Baseline-exception fix: the local `char putchar(char)` shim moved to the
// sim library single definition point in mcs51_uart.cpp.)

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
        printf("[mcs51_buzzer] FAIL: %s\n", msg);
        ++g_fails;
    }
}

uint8_t get_last_pin3_level(void) {
    uint32_t n = wink_mcs51_host_gpio_notify_count();
    for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
        if (wink_mcs51_host_gpio_notify_pin(static_cast<uint32_t>(i)) == 3u) {
            return wink_mcs51_host_gpio_notify_level(static_cast<uint32_t>(i));
        }
    }
    return 0xFFu;
}

}  // namespace

int main(void) {
    printf("[mcs51_buzzer] Starting CMS8S78xx Buzzer unit tests...\n");

    // M1: buzzer is CMS8S-only silicon — select the family explicitly.
    mcs51_test_register_family(MCS51_FAMILY_CMS8S78XX);
    mcs51_context_set_family(MCS51_FAMILY_CMS8S78XX);
    Mcu51Context* ctx = mcs51_get_context();
    mcs51_context_reset(ctx);
    ctx->clock_hz = 24000000u;
    ctx->virtual_us = 0;
    wink_mcs51_host_gpio_notify_reset();

    cms8s_buzzer_init(ctx);

    // Initial state: not running
    check(!cms8s_buzzer_is_running(), "Buzzer should not be running initially");
    check(cms8s_buzzer_next_event_us(ctx) == UINT64_MAX, "next_event_us should be UINT64_MAX initially");

    // Configure buzzer: 10kHz at 24MHz (div8, 150)
    BUZ_ConfigBuzzer(BUZ_CKS_8, 150);
    GPIO_SET_MUX_MODE(P03CFG, GPIO_P03_MUX_BUZZ);

    // Not running until enabled
    check(!cms8s_buzzer_is_running(), "Buzzer should not run before BUZ_EnableBuzzer");

    // Enable buzzer
    BUZ_EnableBuzzer();
    check(cms8s_buzzer_is_running(), "Buzzer should be running after BUZ_EnableBuzzer");
    check(cms8s_buzzer_half_period_us() == 50u, "Half period should be 50us (10kHz square wave)");
    check(get_last_pin3_level() == 1u, "Pin 3 should be driven high on enable");

    uint64_t next_evt = cms8s_buzzer_next_event_us(ctx);
    check(next_evt != UINT64_MAX, "Next event should be scheduled");

    // Advance to next event and poll
    ctx->virtual_us = next_evt;
    cms8s_buzzer_poll(ctx);
    check(get_last_pin3_level() == 0u, "Pin 3 should toggle to 0 at first half-period");
    check(cms8s_buzzer_toggle_count() == 1u, "Toggle count should be 1");
    check(cms8s_buzzer_next_event_us(ctx) == next_evt + 50u, "Next event should advance by 50us");

    // Advance to second event and poll
    ctx->virtual_us = next_evt + 50u;
    cms8s_buzzer_poll(ctx);
    check(get_last_pin3_level() == 1u, "Pin 3 should toggle to 1 at second half-period");
    check(cms8s_buzzer_toggle_count() == 2u, "Toggle count should be 2");

    // Test disable
    BUZ_DisableBuzzer();
    check(!cms8s_buzzer_is_running(), "Buzzer should not be running after disable");
    check(get_last_pin3_level() == 0u, "Pin 3 should be driven low on disable");
    check(cms8s_buzzer_next_event_us(ctx) == UINT64_MAX, "next_event_us should be UINT64_MAX after disable");

    // Test different prescalers: BUZ_CKS_16 -> 100us half-period (5kHz)
    BUZ_ConfigBuzzer(BUZ_CKS_16, 150);
    BUZ_EnableBuzzer();
    check(cms8s_buzzer_half_period_us() == 100u, "Half period for CKS_16 should be 100us (5kHz)");

    // Test BUZ_CKS_32 -> 200us half-period (2.5kHz)
    BUZ_ConfigBuzzer(BUZ_CKS_32, 150);
    check(cms8s_buzzer_half_period_us() == 200u, "Half period for CKS_32 should be 200us (2.5kHz)");

    // Test BUZ_CKS_64 -> 400us half-period (1.25kHz)
    BUZ_ConfigBuzzer(BUZ_CKS_64, 150);
    check(cms8s_buzzer_half_period_us() == 400u, "Half period for CKS_64 should be 400us (1.25kHz)");

    BUZ_DisableBuzzer();

    if (g_fails == 0) {
        printf("[mcs51_buzzer] ALL BUZZER TESTS PASSED!\n");
        return 0;
    }
    printf("[mcs51_buzzer] FAILED: %d checks failed\n", g_fails);
    return 1;
}
