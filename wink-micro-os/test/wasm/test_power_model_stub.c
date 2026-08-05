// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_power_model_stub.c
 * @brief WASM power model API stub unit tests.
 */
#include "unity.h"
#include "pal_wasm_internal.h"

#include <stdint.h>

void setUp(void) {}
void tearDown(void) {}

void test_power_model_api_exists(void) {
    uint64_t energy = pal_wasm_get_total_energy_mj();
    TEST_ASSERT_TRUE(energy >= 0);
}

void test_power_model_pin_api_compiles(void) {
    wasm_pin_power_model_t model = {
        .active_current_ua = 1000,
        .leakage_current_ua = 10,
        .transition_energy_nj = 100
    };
    wink_status_t status = pal_wasm_set_pin_power_model(5, &model);
    TEST_ASSERT_EQUAL(WINK_OK, status);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_power_model_api_exists);
    RUN_TEST(test_power_model_pin_api_compiles);
    return UNITY_END();
}
