// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_i2c_drop_middleware.c
 * @brief WASM pal_i2c_transfer drop middleware unit tests.
 */
#include "unity.h"
#include "pal_hal.h"
#include "pal_wasm_internal.h"
#include "wink_sim_physical.h"
#include "test_physical_golden.h"

#include <emscripten.h>
#include <stdint.h>
#include <stdbool.h>

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

extern void     pal_wasm_set_i2c_drop_permil(uint16_t permil);
extern void     pal_wasm_set_prng_seed(uint32_t seed);
extern uint32_t pal_wasm_get_prng_state(void);
extern void     pal_wasm_reset_physical(void);

static uint8_t  s_write_buf[1] = {0xAA};
static uint8_t  s_read_buf[1]  = {0};

void setUp(void) {
    pal_wasm_reset_physical();
}

void tearDown(void) {}

void test_i2c_zero_drop_does_not_advance_prng(void) {
    uint32_t before = pal_wasm_get_prng_state();
    (void)pal_i2c_transfer(0, 0x50, s_write_buf, 1, s_read_buf, 1);
    (void)pal_i2c_transfer(0, 0x50, s_write_buf, 1, s_read_buf, 1);
    uint32_t after = pal_wasm_get_prng_state();
    TEST_ASSERT_EQUAL_UINT32(before, after);
}

void test_i2c_full_drop_returns_io_err_without_prng_advance(void) {
    pal_wasm_set_i2c_drop_permil(1000u);
    uint32_t before = pal_wasm_get_prng_state();
    wink_status_t r = pal_i2c_transfer(0, 0x50, s_write_buf, 1, s_read_buf, 1);
    uint32_t after = pal_wasm_get_prng_state();
    TEST_ASSERT_EQUAL_INT(WINK_ERR_IO, r);
    TEST_ASSERT_EQUAL_UINT32(before, after);
}

void test_i2c_partial_drop_advances_prng(void) {
    pal_wasm_set_i2c_drop_permil(500u);
    pal_wasm_set_prng_seed(GOLDEN_PRNG_SEED1);
    TEST_ASSERT_EQUAL_UINT32(GOLDEN_PRNG_SEED1, pal_wasm_get_prng_state());

    (void)pal_i2c_transfer(0, 0x50, s_write_buf, 1, s_read_buf, 1);

    TEST_ASSERT_EQUAL_UINT32(GOLDEN_PRNG_AFTER_CALL1, pal_wasm_get_prng_state());
}

void test_i2c_drop_is_deterministic_across_runs(void) {
    pal_wasm_set_i2c_drop_permil(300u);

    pal_wasm_set_prng_seed(42u);
    for (int i = 0; i < 16; i++) {
        (void)pal_i2c_transfer(0, 0x50, s_write_buf, 1, s_read_buf, 1);
    }
    uint32_t state_after_run1 = pal_wasm_get_prng_state();

    pal_wasm_set_prng_seed(42u);
    for (int i = 0; i < 16; i++) {
        (void)pal_i2c_transfer(0, 0x50, s_write_buf, 1, s_read_buf, 1);
    }
    uint32_t state_after_run2 = pal_wasm_get_prng_state();

    TEST_ASSERT_EQUAL_UINT32(state_after_run1, state_after_run2);
    TEST_ASSERT_NOT_EQUAL(42u, state_after_run1);
}

void test_i2c_partial_drop_non_drop_path_still_advances_prng(void) {
    pal_wasm_set_i2c_drop_permil(500u);
    pal_wasm_set_prng_seed(GOLDEN_PRNG_SEED1);
    wink_status_t r = pal_i2c_transfer(0, 0x50, s_write_buf, 1, s_read_buf, 1);
    TEST_ASSERT_TRUE(r == WINK_OK || r == WINK_ERR_IO);
    TEST_ASSERT_NOT_EQUAL(GOLDEN_PRNG_SEED1, pal_wasm_get_prng_state());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_i2c_zero_drop_does_not_advance_prng);
    RUN_TEST(test_i2c_full_drop_returns_io_err_without_prng_advance);
    RUN_TEST(test_i2c_partial_drop_advances_prng);
    RUN_TEST(test_i2c_drop_is_deterministic_across_runs);
    RUN_TEST(test_i2c_partial_drop_non_drop_path_still_advances_prng);
    return UNITY_END();
}
