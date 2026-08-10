// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_pal_gpio_read_wasm_semantics.c
 * @brief P3 contract tests — Arbiter-only pal_gpio_read (no C shadow on read)
 *        + release_mcu on INPUT*.
 *
 * Mini PinArbiter mock via EM_JS (ideal:ui:{N} / mcu:gpio{N} / plugin:*).
 * Build: test/wasm/run_gpio_semantics_emcc.ps1 (emcc + Node).
 */
#include "unity.h"
#include "pal_hal.h"
#include "pal_resource.h"
#include "wasm_bridge.h"
#include "pal_wasm_common.h"

#include <emscripten.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#define PIN 10

extern void pal_wasm_ch1_gpio_reset(void);

/* ─────────────────────────────────────────────────────────
 * Mini PinArbiter mock (matches unisim createUnisimImports id prefixes)
 * ───────────────────────────────────────────────────────── */

EM_JS(void, mock_arbiter_reset, (void), {
    globalThis.__wink_arb = {};
});

EM_JS(void, mock_arbiter_set_driver, (uint16_t pin, const char *id_ptr, uint8_t state), {
    var id = UTF8ToString(id_ptr);
    if (!globalThis.__wink_arb) globalThis.__wink_arb = {};
    var key = String(pin);
    if (!globalThis.__wink_arb[key]) globalThis.__wink_arb[key] = {};
    globalThis.__wink_arb[key][id] = state | 0; /* 0=LOW 1=HIGH */
});

EM_JS(void, mock_arbiter_remove_driver, (uint16_t pin, const char *id_ptr), {
    var id = UTF8ToString(id_ptr);
    if (!globalThis.__wink_arb) return;
    var key = String(pin);
    if (!globalThis.__wink_arb[key]) return;
    delete globalThis.__wink_arb[key][id];
});

EM_JS(uint8_t, mock_arbiter_resolve, (uint16_t pin), {
    if (!globalThis.__wink_arb) return 2; /* HiZ */
    var drivers = globalThis.__wink_arb[String(pin)];
    if (!drivers) return 2;
    var vals = Object.keys(drivers).map(function (k) { return drivers[k] | 0; });
    if (vals.length === 0) return 2;
    var hasLow = vals.indexOf(0) >= 0;
    var hasHigh = vals.indexOf(1) >= 0;
    if (hasLow && hasHigh) return 3; /* CONFLICT */
    if (hasHigh) return 1;
    return 0;
});

EM_JS(uint8_t, js_pal_gpio_read_state, (uint16_t pin), {
    if (typeof globalThis.__wink_arb === 'undefined') return 2;
    var drivers = globalThis.__wink_arb[String(pin)];
    if (!drivers) return 2;
    var vals = Object.keys(drivers).map(function (k) { return drivers[k] | 0; });
    if (vals.length === 0) return 2;
    var hasLow = vals.indexOf(0) >= 0;
    var hasHigh = vals.indexOf(1) >= 0;
    if (hasLow && hasHigh) return 3;
    if (hasHigh) return 1;
    return 0;
});

EM_JS(void, js_pal_gpio_drive_ideal, (uint16_t pin, bool level), {
    if (!globalThis.__wink_arb) globalThis.__wink_arb = {};
    var key = String(pin);
    if (!globalThis.__wink_arb[key]) globalThis.__wink_arb[key] = {};
    globalThis.__wink_arb[key]['ideal:ui:' + pin] = level ? 1 : 0;
});

EM_JS(void, js_pal_gpio_release_ideal, (uint16_t pin), {
    if (!globalThis.__wink_arb) return;
    var key = String(pin);
    if (!globalThis.__wink_arb[key]) return;
    delete globalThis.__wink_arb[key]['ideal:ui:' + pin];
});

EM_JS(void, js_pal_gpio_release_mcu, (uint16_t pin), {
    if (!globalThis.__wink_arb) return;
    var key = String(pin);
    if (!globalThis.__wink_arb[key]) return;
    delete globalThis.__wink_arb[key]['mcu:gpio' + pin];
});

EM_JS(void, js_pal_gpio_write, (uint16_t pin, bool level), {
    if (!globalThis.__wink_arb) globalThis.__wink_arb = {};
    var key = String(pin);
    if (!globalThis.__wink_arb[key]) globalThis.__wink_arb[key] = {};
    globalThis.__wink_arb[key]['mcu:gpio' + pin] = level ? 1 : 0;
});

EM_JS(bool, js_pal_gpio_read, (uint16_t pin), {
    var st = 2;
    if (typeof globalThis.__wink_arb !== 'undefined') {
        var drivers = globalThis.__wink_arb[String(pin)];
        if (drivers) {
            var vals = Object.keys(drivers).map(function (k) { return drivers[k] | 0; });
            if (vals.length > 0) {
                var hasLow = vals.indexOf(0) >= 0;
                var hasHigh = vals.indexOf(1) >= 0;
                if (hasLow && hasHigh) st = 3;
                else if (hasHigh) st = 1;
                else st = 0;
            }
        }
    }
    return (st === 1 || st === 3) ? 1 : 0;
});

EM_JS(void, js_pal_gpio_on_write, (uint8_t pin, uint8_t level), {
    /* no-op notify bridge for unit tests */
});

static void claim_pin(wink_pin_t pin) {
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, (uint32_t)pin, "p1-2"));
}

static bool read_level(wink_pin_t pin) {
    bool lvl = false;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_read(pin, &lvl));
    return lvl;
}

void setUp(void) {
    mock_arbiter_reset();
    pal_resource_reset();
    pal_wasm_ch1_gpio_reset();
    pal_gpio_reset_pin(PIN);
}

void tearDown(void) {}

/* 1) init PULLUP, no drive → HIGH */
void test_pullup_idle_reads_high(void) {
    claim_pin(PIN);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_init(PIN, PAL_GPIO_INPUT_PULLUP));
    TEST_ASSERT_TRUE(read_level(PIN));
}

/* 2) drive LOW → read LOW (not covered by pull) */
void test_drive_ideal_low_beats_pullup(void) {
    claim_pin(PIN);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_init(PIN, PAL_GPIO_INPUT_PULLUP));
    js_pal_gpio_drive_ideal((uint16_t)PIN, false);
    TEST_ASSERT_FALSE(read_level(PIN));
}

/* 3) release ideal → HIGH again */
void test_release_ideal_returns_pullup_high(void) {
    claim_pin(PIN);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_init(PIN, PAL_GPIO_INPUT_PULLUP));
    js_pal_gpio_drive_ideal((uint16_t)PIN, false);
    TEST_ASSERT_FALSE(read_level(PIN));
    js_pal_gpio_release_ideal((uint16_t)PIN);
    TEST_ASSERT_TRUE(read_level(PIN));
}

/* 4) write HIGH then init INPUT_PULLUP → HIGH (MCU released) */
void test_write_high_then_input_pullup_releases_mcu(void) {
    claim_pin(PIN);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_init(PIN, PAL_GPIO_OUTPUT_PUSH_PULL));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_write(PIN, true));
    TEST_ASSERT_EQUAL_UINT8(JS_GPIO_STATE_HIGH, js_pal_gpio_read_state((uint16_t)PIN));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_init(PIN, PAL_GPIO_INPUT_PULLUP));
    TEST_ASSERT_EQUAL_UINT8(JS_GPIO_STATE_HIZ, js_pal_gpio_read_state((uint16_t)PIN));
    TEST_ASSERT_TRUE(read_level(PIN));
}

/* 5) write LOW then init INPUT_PULLUP → HIGH (no ghost LOW) */
void test_write_low_then_input_pullup_no_ghost_low(void) {
    claim_pin(PIN);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_init(PIN, PAL_GPIO_OUTPUT_PUSH_PULL));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_write(PIN, false));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_init(PIN, PAL_GPIO_INPUT_PULLUP));
    TEST_ASSERT_TRUE(read_level(PIN));
}

/* 6) CONFLICT → pal_gpio_read HIGH */
void test_conflict_reads_as_high(void) {
    claim_pin(PIN);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_init(PIN, PAL_GPIO_INPUT_PULLUP));
    mock_arbiter_set_driver((uint16_t)PIN, "plugin:btn", 0);
    mock_arbiter_set_driver((uint16_t)PIN, "ideal:ui:10", 1);
    TEST_ASSERT_EQUAL_UINT8(JS_GPIO_STATE_CONFLICT, js_pal_gpio_read_state((uint16_t)PIN));
    TEST_ASSERT_TRUE(read_level(PIN));
}

/* 7) plugin driver + release_mcu → plugin level remains (driver-id isolation) */
void test_release_mcu_preserves_plugin_driver(void) {
    claim_pin(PIN);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_init(PIN, PAL_GPIO_OUTPUT_PUSH_PULL));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_write(PIN, true));
    mock_arbiter_set_driver((uint16_t)PIN, "plugin:led", 0);
    TEST_ASSERT_EQUAL_UINT8(JS_GPIO_STATE_CONFLICT, js_pal_gpio_read_state((uint16_t)PIN));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_init(PIN, PAL_GPIO_INPUT)); /* release_mcu */
    TEST_ASSERT_EQUAL_UINT8(JS_GPIO_STATE_LOW, js_pal_gpio_read_state((uint16_t)PIN));
    /* Floating INPUT + driven LOW → not DISCONNECTED */
    TEST_ASSERT_FALSE(read_level(PIN));
}

/* 8) drive_ideal then init INPUT_PULLUP → ideal not removed */
void test_init_input_does_not_remove_ideal(void) {
    claim_pin(PIN);
    js_pal_gpio_drive_ideal((uint16_t)PIN, false);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_gpio_init(PIN, PAL_GPIO_INPUT_PULLUP));
    TEST_ASSERT_EQUAL_UINT8(JS_GPIO_STATE_LOW, js_pal_gpio_read_state((uint16_t)PIN));
    TEST_ASSERT_FALSE(read_level(PIN));
}

/* 9) HiZ + mode unknown → LOW, not DISCONNECTED */
void test_hiz_mode_unknown_reads_low_not_disconnected(void) {
    claim_pin(PIN);
    /* deliberately no pal_gpio_init → mode unknown */
    bool lvl = true;
    wink_status_t st = pal_gpio_read(PIN, &lvl);
    TEST_ASSERT_EQUAL_INT(WINK_OK, st);
    TEST_ASSERT_FALSE(lvl);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_pullup_idle_reads_high);
    RUN_TEST(test_drive_ideal_low_beats_pullup);
    RUN_TEST(test_release_ideal_returns_pullup_high);
    RUN_TEST(test_write_high_then_input_pullup_releases_mcu);
    RUN_TEST(test_write_low_then_input_pullup_no_ghost_low);
    RUN_TEST(test_conflict_reads_as_high);
    RUN_TEST(test_release_mcu_preserves_plugin_driver);
    RUN_TEST(test_init_input_does_not_remove_ideal);
    RUN_TEST(test_hiz_mode_unknown_reads_low_not_disconnected);
    return UNITY_END();
}
