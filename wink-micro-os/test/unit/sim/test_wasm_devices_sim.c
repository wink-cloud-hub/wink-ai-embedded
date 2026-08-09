// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_wasm_devices_sim.c
 * @brief Unit tests for WASM target channel simulation models & ABI compatibility.
 */
#include "unity.h"
#include "pal_wasm_common.h"
#include "wasm_bridge.h"
#include <string.h>

extern void pal_wasm_sim_reset_all_devices(void);
extern float pal_wasm_get_pwm_duty_percent(uint8_t channel);
extern bool pal_wasm_get_gpio_output(uint8_t pin);

float js_sim_get_plugin_channel(const char *instance_id, const char *channel_name) {
    (void)instance_id;
    (void)channel_name;
    return -1.0f;
}

void setUp(void) {
    pal_wasm_sim_reset_all_devices();
}

void tearDown(void) {
    pal_wasm_sim_reset_all_devices();
}

void test_abi_reset_all_devices(void) {
    /* G3 ABI export compatibility test */
    pal_wasm_sim_reset_all_devices();
    TEST_ASSERT_EQUAL_FLOAT(0.0f, pal_wasm_get_pwm_duty_percent(0));
    TEST_ASSERT_FALSE(pal_wasm_get_gpio_output(0));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_abi_reset_all_devices);
    return UNITY_END();
}
