// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_debounce_middleware.c
 * @brief WASM pal_gpio_read debounce middleware unit tests.
 */
#include "unity.h"
#include "pal_hal.h"
#include "pal_osal.h"
#include "pal_wasm_internal.h"
#include "wink_sim_physical.h"
#include "test_physical_golden.h"

#include <emscripten.h>
#include <stdint.h>
#include <stdbool.h>

extern void pal_wasm_set_bounce_us(uint32_t us);
extern void pal_wasm_reset_physical(void);
extern void pal_wasm_advance_virtual_clock(uint64_t us);

static bool s_mock_ideal_level = false;
static uint16_t s_mock_last_pin = 0xFFFFu;

EMSCRIPTEN_KEEPALIVE
void test_set_mock_gpio_ideal(uint16_t pin, bool level) {
    s_mock_last_pin = pin;
    s_mock_ideal_level = level;
}

void setUp(void) {
    pal_wasm_reset_physical();
    pal_resource_reset();
    s_mock_ideal_level = false;
    s_mock_last_pin = 0xFFFFu;
}

void tearDown(void) {}

void test_gpio_read_oob_pin_returns_false(void) {
    pal_wasm_set_bounce_us(30000u);
    bool lvl = false;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, pal_gpio_read(WASM_SIM_MAX_PINS, &lvl));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, pal_gpio_read(WASM_SIM_MAX_PINS + 1u, &lvl));
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, pal_gpio_read(65535u, &lvl));
}

void test_gpio_read_oob_pin_no_ctx_mutation(void) {
    pal_wasm_set_bounce_us(30000u);
    bool lvl = false;
    (void)pal_gpio_read(65535u, &lvl);
    wink_phys_debounce_ctx_t *ctx0 = pal_wasm_get_debounce_ctx(0);
    TEST_ASSERT_NOT_NULL(ctx0);
    TEST_ASSERT_FALSE(ctx0->in_bounce);
    TEST_ASSERT_FALSE(ctx0->stable_level);
    TEST_ASSERT_EQUAL_UINT64(0, ctx0->bounce_start_us);
}

void test_gpio_read_zero_bounce_leaves_ctx_clean(void) {
    pal_wasm_set_bounce_us(0u);
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_resource_claim(PAL_RESOURCE_GPIO_PIN, 5u, "test"));
    bool lvl = false;
    (void)pal_gpio_read(5u, &lvl);
    (void)pal_gpio_read(5u, &lvl);
    (void)pal_gpio_read(5u, &lvl);
    wink_phys_debounce_ctx_t *ctx = pal_wasm_get_debounce_ctx(5u);
    TEST_ASSERT_NOT_NULL(ctx);
    TEST_ASSERT_FALSE(ctx->in_bounce);
    TEST_ASSERT_EQUAL_UINT64(0, ctx->bounce_start_us);
}

void test_per_pin_ctx_drives_algorithm_correctly(void) {
    wink_phys_debounce_ctx_t *ctx = pal_wasm_get_debounce_ctx(7u);
    TEST_ASSERT_NOT_NULL(ctx);
    bool step1 = wink_phys_debounce_step(ctx, GOLDEN_BOUNCE_TARGET,
                                         GOLDEN_BOUNCE_NOW1_US, GOLDEN_BOUNCE_US);
    TEST_ASSERT_EQUAL(GOLDEN_BOUNCE_STEP1, step1);
    TEST_ASSERT_TRUE(ctx->in_bounce);
}

void test_distinct_pins_independent_ctx(void) {
    wink_phys_debounce_ctx_t *a = pal_wasm_get_debounce_ctx(3u);
    wink_phys_debounce_ctx_t *b = pal_wasm_get_debounce_ctx(4u);
    (void)wink_phys_debounce_step(a, true, 1000u, 30000u);
    TEST_ASSERT_TRUE(a->in_bounce);
    TEST_ASSERT_FALSE(b->in_bounce);
    TEST_ASSERT_EQUAL_UINT64(0, b->bounce_start_us);
}

void test_gpio_read_last_valid_pin_works(void) {
    pal_wasm_set_bounce_us(30000u);
    wink_phys_debounce_ctx_t *ctx = pal_wasm_get_debounce_ctx(WASM_SIM_MAX_PINS - 1);
    TEST_ASSERT_NOT_NULL(ctx);
    (void)wink_phys_debounce_step(ctx, true, 1000u, 30000u);
    TEST_ASSERT_TRUE(ctx->in_bounce);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_gpio_read_oob_pin_returns_false);
    RUN_TEST(test_gpio_read_oob_pin_no_ctx_mutation);
    RUN_TEST(test_gpio_read_zero_bounce_leaves_ctx_clean);
    RUN_TEST(test_per_pin_ctx_drives_algorithm_correctly);
    RUN_TEST(test_distinct_pins_independent_ctx);
    RUN_TEST(test_gpio_read_last_valid_pin_works);
    return UNITY_END();
}
