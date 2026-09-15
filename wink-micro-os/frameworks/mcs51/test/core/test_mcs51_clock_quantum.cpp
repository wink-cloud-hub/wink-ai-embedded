// SPDX-License-Identifier: GPL-3.0-only
// Task F3: Dynamic microstep quantum calibration from clock_hz test.
#include <stdint.h>
#include <stdio.h>

#include "mcs51_context.h"
#include "wink_mcs51_clock.h"

namespace {

int g_fails = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        printf("[mcs51-quantum] FAIL: %s\n", what);
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

    // ── Test 1: Calibration formula inverse proportionality ───────────────────
    uint32_t step_12m = wink_mcs51_calc_microstep_us(12000000u);
    uint32_t step_24m = wink_mcs51_calc_microstep_us(24000000u);
    uint32_t step_60m = wink_mcs51_calc_microstep_us(60000000u);
    uint32_t step_120m = wink_mcs51_calc_microstep_us(120000000u);

    check(step_12m == 5u, "12 MHz must yield 5 us quantum");
    check(step_24m < step_12m, "24 MHz quantum must be strictly smaller than 12 MHz");
    check(step_60m == 1u, "60 MHz must yield 1 us quantum");
    check(step_120m >= 1u, "120 MHz quantum must not drop below 1 us safety floor");

    // Default when 0 is passed
    check(wink_mcs51_calc_microstep_us(0) == 5u, "0 Hz fallback must yield default 5 us");

    // ── Test 2: Runtime dynamic configuration ─────────────────────────────────
    wink_mcs51_set_clock_hz(24000000u);
    check(wink_mcs51_get_clock_hz() == 24000000u, "Clock Hz must be stored in context");
    check(wink_mcs51_get_microstep_us() == step_24m, "Active microstep must match calibrated quantum");

    // ── Test 3: wink_delay_us exact advance with calibrated quantum ───────────
    uint64_t start_us = ctx->virtual_us;
    wink_delay_us(100u);
    check(ctx->virtual_us == start_us + 100u, "Virtual clock must advance by exact delay amount (100 us)");

    // Odd delay with remainder
    start_us = ctx->virtual_us;
    wink_delay_us(17u);
    check(ctx->virtual_us == start_us + 17u, "Virtual clock must advance by exact remainder without overshoot");

    if (g_fails != 0) {
        printf("[mcs51-quantum] FAILED with %d errors\n", g_fails);
        return 1;
    }
    printf("[mcs51-quantum] PASS: Dynamic microstep quantum calibration verified\n");
    return 0;
}
