/* SPDX-License-Identifier: GPL-3.0-only */
#include "unity.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "soc/soc_caps.h"

static bool s_cb_called = false;
static uint32_t s_cb_duty = 0;

static bool test_fade_cb(const ledc_cb_param_t *param, void *user_arg) {
    (void)user_arg;
    s_cb_called = true;
    if (param) {
        s_cb_duty = param->duty;
    }
    return true;
}

void setUp(void) {
    esp_ledc_reset();
    s_cb_called = false;
    s_cb_duty = 0;
}

void tearDown(void) {
    esp_ledc_reset();
}

void test_ledc_timer_config_validation(void) {
    /* NULL pointer rejected */
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, ledc_timer_config(NULL));

    /* Invalid speed_mode rejected */
    ledc_timer_config_t cfg = {
        .speed_mode = LEDC_SPEED_MODE_MAX,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, ledc_timer_config(&cfg));

    /* Zero frequency rejected */
    cfg.speed_mode = LEDC_LOW_SPEED_MODE;
    cfg.freq_hz = 0;
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, ledc_timer_config(&cfg));

    /* Zero resolution rejected */
    cfg.freq_hz = 5000;
    cfg.duty_resolution = (ledc_timer_bit_t)0;
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, ledc_timer_config(&cfg));

    /* Valid config succeeds */
    cfg.duty_resolution = LEDC_TIMER_13_BIT;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, ledc_timer_config(&cfg));
    TEST_ASSERT_EQUAL_UINT32(5000, ledc_get_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0));
}

void test_ledc_channel_without_timer_rejected(void) {
    ledc_channel_config_t ch_cfg = {
        .gpio_num = 2,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
        .flags = 0,
        .sleep_mode = 0
    };
    /* Timer 0 not configured yet -> ESP_ERR_INVALID_STATE */
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_STATE, ledc_channel_config(&ch_cfg));
}

void test_ledc_channel_config_and_duty(void) {
    ledc_timer_config_t t_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT, /* top = 1023 */
        .timer_num = LEDC_TIMER_1,
        .freq_hz = 1000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    TEST_ASSERT_EQUAL_INT32(ESP_OK, ledc_timer_config(&t_cfg));

    ledc_channel_config_t ch_cfg = {
        .gpio_num = 4,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_1,
        .duty = 512, /* approx 50% */
        .hpoint = 0,
        .flags = 0,
        .sleep_mode = 0
    };
    TEST_ASSERT_EQUAL_INT32(ESP_OK, ledc_channel_config(&ch_cfg));
    TEST_ASSERT_EQUAL_UINT32(512, ledc_get_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1));

    /* Update to 0 duty */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1));
    TEST_ASSERT_EQUAL_UINT32(0, ledc_get_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1));

    /* Update to max duty (1023) */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 1023));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1));
    TEST_ASSERT_EQUAL_UINT32(1023, ledc_get_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1));

    /* Over-max duty clamped */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 2048));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1));
    TEST_ASSERT_EQUAL_UINT32(2048, ledc_get_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1));

    /* Stop channel */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0));
    TEST_ASSERT_EQUAL_UINT32(0, ledc_get_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1));
}

void test_ledc_channel_boundary_checks(void) {
    /* Channel out of bounds */
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG,
        ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)SOC_LEDC_CHANNEL_NUM, 100));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG,
        ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)SOC_LEDC_CHANNEL_NUM));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG,
        ledc_stop(LEDC_LOW_SPEED_MODE, (ledc_channel_t)SOC_LEDC_CHANNEL_NUM, 0));
    TEST_ASSERT_EQUAL_UINT32(0,
        ledc_get_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)SOC_LEDC_CHANNEL_NUM));
}

void test_ledc_fade_workflow_and_callback(void) {
    ledc_timer_config_t t_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_2,
        .freq_hz = 2000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    TEST_ASSERT_EQUAL_INT32(ESP_OK, ledc_timer_config(&t_cfg));

    ledc_channel_config_t ch_cfg = {
        .gpio_num = 18,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_2,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_2,
        .duty = 0,
        .hpoint = 0,
        .flags = 0,
        .sleep_mode = 0
    };
    TEST_ASSERT_EQUAL_INT32(ESP_OK, ledc_channel_config(&ch_cfg));

    TEST_ASSERT_EQUAL_INT32(ESP_OK, ledc_fade_func_install(0));

    ledc_cbs_t cbs = { .fade_cb = test_fade_cb };
    TEST_ASSERT_EQUAL_INT32(ESP_OK, ledc_cb_register(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, &cbs, NULL));

    TEST_ASSERT_EQUAL_INT32(ESP_OK,
        ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, 128, 500));
    TEST_ASSERT_EQUAL_INT32(ESP_OK,
        ledc_fade_start(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, LEDC_FADE_NO_WAIT));

    TEST_ASSERT_TRUE(s_cb_called);
    TEST_ASSERT_EQUAL_UINT32(128, s_cb_duty);
    TEST_ASSERT_EQUAL_UINT32(128, ledc_get_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2));

    ledc_fade_func_uninstall();
}

void test_ledc_dynamic_frequency_adjustment(void) {
    ledc_timer_config_t t_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_3,
        .freq_hz = 1000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    TEST_ASSERT_EQUAL_INT32(ESP_OK, ledc_timer_config(&t_cfg));

    ledc_channel_config_t ch_cfg = {
        .gpio_num = 19,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_3,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_3,
        .duty = 100,
        .hpoint = 0,
        .flags = 0,
        .sleep_mode = 0
    };
    TEST_ASSERT_EQUAL_INT32(ESP_OK, ledc_channel_config(&ch_cfg));

    TEST_ASSERT_EQUAL_UINT32(1000, ledc_get_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_3));

    /* Adjust frequency dynamically */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_3, 4000));
    TEST_ASSERT_EQUAL_UINT32(4000, ledc_get_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_3));

    /* Unconfigured timer set freq rejected */
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_STATE,
        ledc_set_freq(LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0, 2000));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_ledc_timer_config_validation);
    RUN_TEST(test_ledc_channel_without_timer_rejected);
    RUN_TEST(test_ledc_channel_config_and_duty);
    RUN_TEST(test_ledc_channel_boundary_checks);
    RUN_TEST(test_ledc_fade_workflow_and_callback);
    RUN_TEST(test_ledc_dynamic_frequency_adjustment);
    return UNITY_END();
}
