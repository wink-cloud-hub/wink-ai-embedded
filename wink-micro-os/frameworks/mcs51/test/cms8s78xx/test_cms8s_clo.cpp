// SPDX-License-Identifier: GPL-3.0-only
// Unit tests for the CMS8S78xx system-clock output (CLO) peripheral model.
#include <stdint.h>
#include <stdio.h>

#include "cms8s78xx.h"
#include "cms8s_clo.h"
#include "mcs51_context.h"
#include "mcs51_test_harness.h"
#include "wink_mcs51_gpio.h"

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
        printf("[mcs51_clo] FAIL: %s\n", msg);
        ++g_fails;
    }
}

// First pin-11 (P1.3/CLO) level ever driven (forward scan). The host notify
// log holds only 128 entries and saturates, so bulk toggles are asserted
// through cms8s_clo_toggle_count(), never by scanning hundreds of edges.
uint8_t get_first_pin11_level(void) {
    uint32_t n = wink_mcs51_host_gpio_notify_count();
    if (n > 128u) {
        n = 128u;
    }
    for (uint32_t i = 0u; i < n; ++i) {
        if (wink_mcs51_host_gpio_notify_pin(i) == 11u) {
            return wink_mcs51_host_gpio_notify_level(i);
        }
    }
    return 0xFFu;
}

uint8_t get_last_pin11_level(void) {
    uint32_t n = wink_mcs51_host_gpio_notify_count();
    if (n > 128u) {
        n = 128u;
    }
    for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
        if (wink_mcs51_host_gpio_notify_pin(static_cast<uint32_t>(i))
            == 11u) {
            return wink_mcs51_host_gpio_notify_level(
                static_cast<uint32_t>(i));
        }
    }
    return 0xFFu;
}

void enable_clo(Mcu51Context* ctx) {
    SYS_SET_SYSTEM_CLK(SYS_CLK_DIV_1);
    GPIO_SET_MUX_MODE(P13CFG, GPIO_P13_MUX_CLO);
    cms8s_clo_poll(ctx);
}

}  // namespace

int main(void) {
    printf("[mcs51_clo] Starting CMS8S78xx CLO unit tests...\n");

    mcs51_test_register_family(MCS51_FAMILY_CMS8S78XX);
    mcs51_context_set_family(MCS51_FAMILY_CMS8S78XX);
    Mcu51Context* ctx = mcs51_get_context();
    mcs51_context_reset(ctx);
    ctx->clock_hz = 24000000u;
    ctx->virtual_us = 0u;
    wink_mcs51_host_gpio_notify_reset();

    cms8s_clo_init(ctx);

    // ── Test 1: initial idle ──
    check(!cms8s_clo_is_running(), "T1: CLO must not run initially");
    check(cms8s_clo_next_event_us(ctx) == UINT64_MAX,
        "T1: next_event_us must be UINT64_MAX initially");
    check(cms8s_clo_toggle_count() == 0u, "T1: toggle count must be 0");
    check(get_first_pin11_level() == 0xFFu, "T1: pin 11 must be undriven");

    // ── Test 2: DIV_1 + mux CLO starts the Fsys/64 output ──
    enable_clo(ctx);
    check(ctx->clock_hz == 24000000u, "T2: DIV_1 must hold Fsys at 24MHz");
    check(cms8s_clo_is_running(), "T2: CLO must run after mux CLO");
    check(cms8s_clo_half_period_us() == 1u,
        "T2: Bresenham base step must be 1us at 24MHz");
    check(get_first_pin11_level() == 1u,
        "T2: pin 11 first drive must be high");
    // Proxy writes above run microsteps that already advanced virtual_us
    // and toggled the pin: re-baseline for deterministic stepping below.
    cms8s_clo_reset(ctx);
    wink_mcs51_host_gpio_notify_reset();
    ctx->virtual_us = 0u;
    cms8s_clo_poll(ctx);
    check(cms8s_clo_is_running(), "T2: CLO must run after re-baseline");
    check(get_first_pin11_level() == 1u,
        "T2: pin 11 re-baselined drive must be high");
    check(cms8s_clo_next_event_us(ctx) == 1u,
        "T2: first toggle must be scheduled at 1us");

    // ── Test 3: Bresenham stepping [1, 1, 2]us, 3 toggles in 4us ──
    ctx->virtual_us = 1u;
    cms8s_clo_poll(ctx);
    check(cms8s_clo_toggle_count() == 1u, "T3: toggle count must be 1");
    check(cms8s_clo_next_event_us(ctx) == 2u,
        "T3: second interval must be 1us (next at 2us)");
    ctx->virtual_us = 2u;
    cms8s_clo_poll(ctx);
    check(cms8s_clo_toggle_count() == 2u, "T3: toggle count must be 2");
    check(cms8s_clo_next_event_us(ctx) == 4u,
        "T3: third interval must carry +1 (next at 4us)");
    ctx->virtual_us = 4u;
    cms8s_clo_poll(ctx);
    check(cms8s_clo_toggle_count() == 3u, "T3: toggle count must be 3");
    check(cms8s_clo_next_event_us(ctx) == 5u,
        "T3: next event must advance to 5us");

    // ── Test 4: mux back to GPIO stops the output ──
    GPIO_SET_MUX_MODE(P13CFG, GPIO_MUX_GPIO);
    cms8s_clo_poll(ctx);
    check(!cms8s_clo_is_running(), "T4: CLO must stop off mux CLO");
    check(get_last_pin11_level() == 0u,
        "T4: pin 11 must park at safe level 0");
    check(cms8s_clo_next_event_us(ctx) == UINT64_MAX,
        "T4: next_event_us must be UINT64_MAX after stop");

    // ── Test 5: dynamic retune follows clock_hz (6MHz -> 93.75kHz) ──
    GPIO_SET_MUX_MODE(P13CFG, GPIO_P13_MUX_CLO);
    ctx->clock_hz = 6000000u;
    // Same microstep re-baseline as T2: the mux write above already
    // polled and toggled, so reset + restart from a known instant.
    cms8s_clo_reset(ctx);
    wink_mcs51_host_gpio_notify_reset();
    ctx->virtual_us = 4u;
    cms8s_clo_poll(ctx);
    const uint32_t base = cms8s_clo_toggle_count();
    check(cms8s_clo_is_running(), "T5: CLO must restart on mux CLO");
    check(cms8s_clo_half_period_us() == 5u,
        "T5: base step must be 5us at 6MHz (32e6/6e6)");
    check(cms8s_clo_next_event_us(ctx) == 9u,
        "T5: restart at 4us must schedule 9us (+5)");
    ctx->virtual_us = 9u;
    cms8s_clo_poll(ctx);
    check(cms8s_clo_toggle_count() == base + 1u, "T5: count must be base+1");
    check(cms8s_clo_next_event_us(ctx) == 14u, "T5: next must be 14us (+5)");
    ctx->virtual_us = 14u;
    cms8s_clo_poll(ctx);
    check(cms8s_clo_toggle_count() == base + 2u, "T5: count must be base+2");
    check(cms8s_clo_next_event_us(ctx) == 20u,
        "T5: accumulator carry must stretch to 20us (+6)");
    ctx->virtual_us = 20u;
    cms8s_clo_poll(ctx);
    check(cms8s_clo_toggle_count() == base + 3u, "T5: count must be base+3");

    // ── Test 6: classic-family isolation (ADR-0004 gate) ──
    mcs51_context_set_family(MCS51_FAMILY_CLASSIC);
    cms8s_clo_poll(ctx);  // must be a safe no-op, never crash
    check(cms8s_clo_next_event_us(ctx) == UINT64_MAX,
        "T6: classic poll must stay idle (UINT64_MAX)");
    check(!cms8s_clo_is_running(), "T6: classic must report not running");
    check(cms8s_clo_toggle_count() == 0u,
        "T6: classic must report neutral count 0");

    // ── Test 7: reset-in-flight clears cleanly ──
    mcs51_context_set_family(MCS51_FAMILY_CMS8S78XX);
    mcs51_context_reset(ctx);
    ctx->clock_hz = 24000000u;
    ctx->virtual_us = 0u;
    cms8s_clo_init(ctx);
    enable_clo(ctx);
    check(cms8s_clo_is_running(), "T7: CLO must run before reset");
    // Re-baseline past the config-write microsteps (same as T2/T5).
    cms8s_clo_reset(ctx);
    wink_mcs51_host_gpio_notify_reset();
    ctx->virtual_us = 0u;
    cms8s_clo_poll(ctx);
    check(cms8s_clo_is_running(), "T7: CLO must run after re-baseline");
    ctx->virtual_us = 1u;
    cms8s_clo_poll(ctx);
    ctx->virtual_us = 2u;
    cms8s_clo_poll(ctx);
    check(cms8s_clo_toggle_count() == 2u, "T7: count must be 2 in flight");
    cms8s_clo_reset(ctx);
    check(!cms8s_clo_is_running(), "T7: reset must stop the output");
    check(cms8s_clo_toggle_count() == 0u, "T7: reset must clear count");
    check(cms8s_clo_half_period_us() == 0u, "T7: reset must clear step");
    check(cms8s_clo_next_event_us(ctx) == UINT64_MAX,
        "T7: reset must clear next event");
    check(get_last_pin11_level() == 0u,
        "T7: reset must park pin 11 at safe level 0");

    if (g_fails == 0) {
        printf("[mcs51_clo] ALL CLO TESTS PASSED!\n");
        return 0;
    }
    printf("[mcs51_clo] FAILED: %d checks failed\n", g_fails);
    return 1;
}
