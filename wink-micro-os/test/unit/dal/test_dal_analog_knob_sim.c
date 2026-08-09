// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_dal_analog_knob_sim.c
 * @brief Unit tests for Wasm Target Channel 3 Analog ADC simulation path.
 */
#include "unity.h"
#include "wink_status.h"
#include "hal/pal_adc.h"
#include "pal_resource.h"

extern void pal_wasm_ch3_adc_reset(void);
extern void pal_wasm_set_fidelity_level(uint8_t level);

void setUp(void)
{
    pal_resource_reset();
    pal_wasm_ch3_adc_reset();
    pal_wasm_set_fidelity_level(0); /* Ideal mode */
}

void tearDown(void)
{
    pal_resource_reset();
    pal_wasm_ch3_adc_reset();
}

void test_adc_init_and_read_raw(void)
{
    pal_adc_config_t cfg = {
        .pin = 34,
        .full_scale_mv = 3300,
        .resolution_bits = 12,
    };

    pal_adc_channel_t ch = 0;
    wink_status_t st = pal_adc_init(ch, &cfg);
    TEST_ASSERT_EQUAL_INT(WINK_OK, st);

    uint16_t out_mv = 0;
    st = pal_adc_read_mv(ch, &out_mv);
    TEST_ASSERT_EQUAL_INT(WINK_OK, st);

    pal_adc_deinit(ch);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_adc_init_and_read_raw);
    return UNITY_END();
}
