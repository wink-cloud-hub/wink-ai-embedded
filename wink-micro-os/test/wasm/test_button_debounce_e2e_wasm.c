// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_button_debounce_e2e_wasm.c
 * @brief End-to-end button debounce WASM simulation unit tests.
 */
#include "unity.h"
#include "dal_button.h"
#include "pal_hal.h"
#include "pal_osal.h"
#include "pal_resource.h"
#include "pal_wasm_internal.h"
#include "wink_sim_physical.h"

#include <emscripten.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

extern void pal_wasm_set_bounce_us(uint32_t us);
extern void pal_wasm_set_prng_seed(uint32_t seed);
extern void pal_wasm_reset_physical(void);
extern void pal_wasm_advance_virtual_clock(uint64_t us);

#define TICK_MS 10
#define BOUNCE_US 30000u

EM_JS(bool, js_pal_gpio_read, (uint16_t pin), {
    if (typeof globalThis.__wink_gpio_ideal === 'undefined') {
        globalThis.__wink_gpio_ideal = {};
    }
    var v = globalThis.__wink_gpio_ideal[pin];
    return (v === true) ? 1 : 0;
});

EM_JS(uint8_t, js_pal_gpio_read_state, (uint16_t pin), {
    if (typeof globalThis.__wink_gpio_ideal === 'undefined') {
        globalThis.__wink_gpio_ideal = {};
    }
    if (!Object.prototype.hasOwnProperty.call(globalThis.__wink_gpio_ideal, pin)) {
        return 2;
    }
    return globalThis.__wink_gpio_ideal[pin] ? 1 : 0;
});

EM_JS(void, js_pal_gpio_drive_ideal, (uint16_t pin, bool level), {
    if (typeof globalThis.__wink_gpio_ideal === 'undefined') {
        globalThis.__wink_gpio_ideal = {};
    }
    globalThis.__wink_gpio_ideal[pin] = (level ? true : false);
});

EM_JS(void, js_pal_gpio_release_ideal, (uint16_t pin), {
    if (typeof globalThis.__wink_gpio_ideal === 'undefined') return;
    delete globalThis.__wink_gpio_ideal[pin];
});

EM_JS(void, js_pal_gpio_release_mcu, (uint16_t pin), {
});

EM_JS(void, test_set_gpio_ideal_js, (uint16_t pin, bool level), {
    if (typeof globalThis.__wink_gpio_ideal === 'undefined') {
        globalThis.__wink_gpio_ideal = {};
    }
    globalThis.__wink_gpio_ideal[pin] = (level ? true : false);
});

EM_JS(void, test_clear_gpio_ideal_js, (void), {
    globalThis.__wink_gpio_ideal = {};
});

static bool s_pin_registered[WASM_SIM_MAX_PINS];

static void wasm_set_gpio_ideal(uint16_t pin, bool level) {
    test_set_gpio_ideal_js(pin, level);
    if (pin >= WASM_SIM_MAX_PINS) { return; }
    if (!s_pin_registered[pin]) {
        s_pin_registered[pin] = true;
        wink_phys_debounce_ctx_t *ctx = pal_wasm_get_debounce_ctx(pin);
        if (ctx != NULL) {
            ctx->stable_level    = level;
            ctx->in_bounce       = false;
            ctx->bounce_start_us = 0;
            ctx->bounce_flip     = false;
        }
    }
}

static void wasm_clear_gpio_ideal(void) {
    test_clear_gpio_ideal_js();
    memset(s_pin_registered, 0, sizeof(s_pin_registered));
}

void setUp(void) {
    pal_wasm_reset_physical();
    pal_resource_reset();
    wasm_clear_gpio_ideal();
}

void tearDown(void) {}

static void run_ticks(dal_button_t *btn, int n) {
    for (int i = 0; i < n; i++) {
        pal_wasm_advance_virtual_clock((uint64_t)TICK_MS * 1000u);
        TEST_ASSERT_EQUAL(WINK_OK, dal_button_poll(btn));
    }
}

static bool raw_pressed(uint16_t pin, bool active_low) {
    bool lvl = false;
    WINK_IGNORE_UNUSED(pal_gpio_read((wink_pin_t)pin, &lvl));
    return lvl != active_low;
}

void test_dal_button_absorbs_bounce_and_settles(void) {
    pal_wasm_set_bounce_us(BOUNCE_US);
    pal_wasm_set_prng_seed(1u);

    dal_button_t btn;
    const dal_button_config_t cfg = { .owner = "wasm_e2e_debounce_bounce", .pin = 7, .active_low = true };
    TEST_ASSERT_EQUAL(WINK_OK, dal_button_init(&btn, &cfg));

    wasm_set_gpio_ideal(7, true);
    run_ticks(&btn, 2);
    bool released = true;
    TEST_ASSERT_EQUAL(WINK_OK, dal_button_is_pressed(&btn, &released));
    TEST_ASSERT_FALSE(released);

    wasm_set_gpio_ideal(7, false);
    run_ticks(&btn, 6);

    bool pressed = false;
    TEST_ASSERT_EQUAL(WINK_OK, dal_button_is_pressed(&btn, &pressed));
    TEST_ASSERT_TRUE(pressed);
}

void test_raw_read_without_debounce_bounces(void) {
    pal_wasm_set_bounce_us(BOUNCE_US);
    pal_wasm_set_prng_seed(1u);

    wasm_set_gpio_ideal(9, true);
    pal_wasm_advance_virtual_clock((uint64_t)TICK_MS * 1000u);
    wasm_set_gpio_ideal(9, false);

    bool saw_pressed = false, saw_released = false;
    for (int i = 0; i < 3; i++) {
        if (raw_pressed(9, true)) { saw_pressed = true; }
        else { saw_released = true; }
        pal_wasm_advance_virtual_clock((uint64_t)TICK_MS * 1000u);
    }
    TEST_ASSERT_TRUE(saw_pressed && saw_released);
}

void test_no_bounce_config_settles_fast(void) {
    dal_button_t btn;
    const dal_button_config_t cfg = { .owner = "wasm_e2e_debounce_baseline", .pin = 8, .active_low = false };
    TEST_ASSERT_EQUAL(WINK_OK, dal_button_init(&btn, &cfg));
    wasm_set_gpio_ideal(8, true);
    run_ticks(&btn, 5);
    bool pressed = false;
    TEST_ASSERT_EQUAL(WINK_OK, dal_button_is_pressed(&btn, &pressed));
    TEST_ASSERT_TRUE(pressed);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_dal_button_absorbs_bounce_and_settles);
    RUN_TEST(test_raw_read_without_debounce_bounces);
    RUN_TEST(test_no_bounce_config_settles_fast);
    return UNITY_END();
}
