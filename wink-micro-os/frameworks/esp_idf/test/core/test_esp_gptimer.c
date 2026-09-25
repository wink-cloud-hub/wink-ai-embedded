/* SPDX-License-Identifier: GPL-3.0-only */
#include "unity.h"
#include "driver/gptimer.h"
#include "hal/pal_hwtimer.h"
#include "esp_err.h"
#include "esp_idf_wink.h"

static bool s_alarm_fired = false;
static uint64_t s_alarm_val = 0;

static bool test_gptimer_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
    (void)timer;
    (void)user_ctx;
    s_alarm_fired = true;
    if (edata) {
        s_alarm_val = edata->alarm_value;
    }
    return true;
}

void setUp(void) {
    esp_gptimer_reset();
    s_alarm_fired = false;
    s_alarm_val = 0;
}

void tearDown(void) {
    esp_gptimer_reset();
}

void test_gptimer_lifecycle_and_validation(void) {
    /* Invalid arguments */
    gptimer_handle_t timer = NULL;
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, gptimer_new_timer(NULL, &timer));

    gptimer_config_t cfg = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 0
    };
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, gptimer_new_timer(&cfg, &timer));

    /* Valid creation */
    cfg.resolution_hz = 1000000; // 1MHz
    TEST_ASSERT_EQUAL_INT32(ESP_OK, gptimer_new_timer(&cfg, &timer));
    TEST_ASSERT_NOT_NULL(timer);

    TEST_ASSERT_EQUAL_INT32(ESP_OK, gptimer_enable(timer));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, gptimer_start(timer));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, gptimer_stop(timer));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, gptimer_disable(timer));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, gptimer_del_timer(timer));
}

void test_gptimer_raw_count(void) {
    gptimer_config_t cfg = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000
    };
    gptimer_handle_t timer = NULL;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, gptimer_new_timer(&cfg, &timer));

    uint64_t count = 0;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, gptimer_get_raw_count(timer, &count));

    /* Setting count */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, gptimer_set_raw_count(timer, 50000));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, gptimer_get_raw_count(timer, &count));
    TEST_ASSERT_TRUE(count >= 50000);

    TEST_ASSERT_EQUAL_INT32(ESP_OK, gptimer_del_timer(timer));
}

void test_gptimer_alarm_and_callback(void) {
    gptimer_config_t cfg = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000
    };
    gptimer_handle_t timer = NULL;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, gptimer_new_timer(&cfg, &timer));

    gptimer_event_callbacks_t cbs = {
        .on_alarm = test_gptimer_cb
    };
    TEST_ASSERT_EQUAL_INT32(ESP_OK, gptimer_register_event_callbacks(timer, &cbs, NULL));

    gptimer_alarm_config_t alarm_cfg = {
        .alarm_count = 20000,
        .reload_count = 0,
        .flags = { .auto_reload_on_alarm = false }
    };
    TEST_ASSERT_EQUAL_INT32(ESP_OK, gptimer_set_alarm_action(timer, &alarm_cfg));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, gptimer_enable(timer));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, gptimer_start(timer));

    /* Test fire soft */
    pal_hwtimer_fire_soft(0);
    TEST_ASSERT_TRUE(s_alarm_fired);
    TEST_ASSERT_TRUE(s_alarm_val == 20000);

    TEST_ASSERT_EQUAL_INT32(ESP_OK, gptimer_stop(timer));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, gptimer_del_timer(timer));
}

void test_gptimer_pool_limit(void) {
    gptimer_config_t cfg = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000
    };
    gptimer_handle_t timers[PAL_HWTIMERS_MAX];
    for (int i = 0; i < PAL_HWTIMERS_MAX; i++) {
        TEST_ASSERT_EQUAL_INT32(ESP_OK, gptimer_new_timer(&cfg, &timers[i]));
    }

    gptimer_handle_t extra = NULL;
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_NO_MEM, gptimer_new_timer(&cfg, &extra));

    for (int i = 0; i < PAL_HWTIMERS_MAX; i++) {
        TEST_ASSERT_EQUAL_INT32(ESP_OK, gptimer_del_timer(timers[i]));
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_gptimer_lifecycle_and_validation);
    RUN_TEST(test_gptimer_raw_count);
    RUN_TEST(test_gptimer_alarm_and_callback);
    RUN_TEST(test_gptimer_pool_limit);
    return UNITY_END();
}
