// SPDX-License-Identifier: Apache-2.0
// Task F4: Channel 1b Soft PWM duty cycle measurement test (< 2% error).
#include <stdint.h>
#include <stdio.h>
#include <cmath>

#include "mcs51_context.h"
#include "wink_mcs51_clock.h"
#include "wink_mcs51_pwm_meter.h"

extern "C" void js_pal_gpio_write(uint16_t pin, bool level, uint8_t strength);

namespace {

int g_fails = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        printf("[mcs51-soft-pwm] FAIL: %s\n", what);
        ++g_fails;
    }
}

} // namespace

extern "C" void wink_mcs51_user_main(void) {}
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

int main(void) {
    Mcu51Context* ctx = mcs51_get_context();
    mcs51_context_reset(ctx);

    const uint16_t TEST_PIN = 8; // P1.0

    // ── Test 1: 30% Duty Cycle Measurement ────────────────────────────────────
    // 300 us HIGH, 700 us LOW per 1000 us period, repeated 10 times = 10,000 us
    wink_mcs51_pwm_meter_reset(TEST_PIN);
    wink_mcs51_pwm_meter_start(TEST_PIN);

    for (int cycle = 0; cycle < 10; ++cycle) {
        // Drive HIGH for 300 us
        js_pal_gpio_write(TEST_PIN, true, 3);
        wink_mcs51_test_advance_virtual_us(300);

        // Drive LOW for 700 us
        js_pal_gpio_write(TEST_PIN, false, 3);
        wink_mcs51_test_advance_virtual_us(700);
    }

    float duty_30 = wink_mcs51_pwm_meter_get_duty_cycle(TEST_PIN);
    float err_30 = std::fabs(duty_30 - 30.0f);
    printf("[mcs51-soft-pwm] Target: 30.0%%, Measured: %.3f%%, Error: %.3f%%\n", duty_30, err_30);
    check(err_30 < 2.0f, "30% PWM measurement error must be strictly < 2.0%");

    wink_mcs51_pwm_meter_stop(TEST_PIN);

    // ── Test 2: 75% Duty Cycle Measurement ────────────────────────────────────
    // 750 us HIGH, 250 us LOW per 1000 us period, repeated 10 times
    wink_mcs51_pwm_meter_reset(TEST_PIN);
    wink_mcs51_pwm_meter_start(TEST_PIN);

    for (int cycle = 0; cycle < 10; ++cycle) {
        // Drive HIGH for 750 us
        js_pal_gpio_write(TEST_PIN, true, 3);
        wink_mcs51_test_advance_virtual_us(750);

        // Drive LOW for 250 us
        js_pal_gpio_write(TEST_PIN, false, 3);
        wink_mcs51_test_advance_virtual_us(250);
    }

    float duty_75 = wink_mcs51_pwm_meter_get_duty_cycle(TEST_PIN);
    float err_75 = std::fabs(duty_75 - 75.0f);
    printf("[mcs51-soft-pwm] Target: 75.0%%, Measured: %.3f%%, Error: %.3f%%\n", duty_75, err_75);
    check(err_75 < 2.0f, "75% PWM measurement error must be strictly < 2.0%");

    wink_mcs51_pwm_meter_stop(TEST_PIN);

    // ── Test 3: Transitions count and reset ────────────────────────────────────
    check(wink_mcs51_pwm_meter_get_transitions(TEST_PIN) == 20, "Must record 20 transitions for 10 cycles");
    wink_mcs51_pwm_meter_reset(TEST_PIN);
    check(wink_mcs51_pwm_meter_get_transitions(TEST_PIN) == 0, "Transitions must be 0 after reset");

    if (g_fails != 0) {
        printf("[mcs51-soft-pwm] FAILED with %d errors\n", g_fails);
        return 1;
    }
    printf("[mcs51-soft-pwm] PASS: Soft PWM duty cycle measurement verified\n");
    return 0;
}
