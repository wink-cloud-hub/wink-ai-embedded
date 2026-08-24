// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_pal_adc_continuous.c
 * @brief PAL ADC continuous mode with DMA streaming unit tests.
 */
#include "unity.h"
#include "hal/pal_adc.h"

static bool s_half_full_called = false;
static bool s_full_called = false;
static size_t s_last_sample_count = 0;

static void test_on_half_full(void *arg, const uint16_t *buf, size_t n) {
    (void)arg;
    (void)buf;
    s_half_full_called = true;
    s_last_sample_count = n;
}

static void test_on_full(void *arg, const uint16_t *buf, size_t n) {
    (void)arg;
    (void)buf;
    s_full_called = true;
    s_last_sample_count = n;
}

void setUp(void) {
    s_half_full_called = false;
    s_full_called = false;
    s_last_sample_count = 0;
}

void tearDown(void) {
    pal_adc_continuous_stop(0);
}

void test_adc_continuous_start_stop(void) {
    uint16_t dma_a[64];
    uint16_t dma_b[64];
    const uint8_t channels[2] = {0, 1};

    pal_adc_continuous_cfg_t cfg = {
        .source = PAL_ADC_TRIG_SOURCE_SW,
        .adc_unit = 0,
        .channels = channels,
        .channel_count = 2,
        .edge = PAL_ADC_TRIG_AT_PWM_VALLEY,
        .sampling_period_pwm = 1,
        .dma_buf_a = dma_a,
        .dma_buf_b = dma_b,
        .samples_per_buf = 64,
        .on_half_full = test_on_half_full,
        .on_full = test_on_full,
        .cb_arg = NULL,
    };

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_continuous_start(&cfg));
    TEST_ASSERT_TRUE(s_half_full_called);
    TEST_ASSERT_TRUE(s_full_called);
    TEST_ASSERT_EQUAL_UINT32(64, s_last_sample_count);

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_adc_continuous_stop(0));
}

void test_adc_continuous_invalid_args(void) {
    uint16_t dma_a[64];
    const uint8_t channels[2] = {0, 1};

    /* NULL config */
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, pal_adc_continuous_start(NULL));

    /* NULL buffer */
    pal_adc_continuous_cfg_t cfg = {
        .source = PAL_ADC_TRIG_SOURCE_SW,
        .adc_unit = 0,
        .channels = channels,
        .channel_count = 2,
        .dma_buf_a = NULL,
        .samples_per_buf = 64,
    };
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, pal_adc_continuous_start(&cfg));

    /* Zero sample count */
    cfg.dma_buf_a = dma_a;
    cfg.samples_per_buf = 0;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, pal_adc_continuous_start(&cfg));

    /* Invalid unit */
    cfg.samples_per_buf = 64;
    cfg.adc_unit = 99;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, pal_adc_continuous_start(&cfg));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_adc_continuous_start_stop);
    RUN_TEST(test_adc_continuous_invalid_args);
    return UNITY_END();
}
