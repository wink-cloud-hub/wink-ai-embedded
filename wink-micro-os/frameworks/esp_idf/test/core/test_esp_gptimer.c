/* SPDX-License-Identifier: GPL-3.0-only */
#include "unity.h"
#include "driver/gptimer.h"
#include "hal/pal_hwtimer.h"
#include "esp_err.h"
#include "esp_idf_wink.h"
#include "esp_sim_handle.h"

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

void test_gptimer_stale_handle_and_aba(void) {
    gptimer_config_t cfg = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000
    };
    gptimer_handle_t t1 = NULL;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, gptimer_new_timer(&cfg, &t1));
    TEST_ASSERT_NOT_NULL(t1);

    /* Resolution query works */
    uint32_t res = 0;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, gptimer_get_resolution(t1, &res));
    TEST_ASSERT_EQUAL_UINT32(1000000, res);

    /* Delete t1 */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, gptimer_del_timer(t1));

    /* Operating on deleted t1 must return ESP_ERR_INVALID_ARG */
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, gptimer_start(t1));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, gptimer_stop(t1));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, gptimer_enable(t1));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, gptimer_disable(t1));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, gptimer_del_timer(t1));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, gptimer_get_resolution(t1, &res));

    /* Create t2, reusing slot 0 */
    gptimer_handle_t t2 = NULL;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, gptimer_new_timer(&cfg, &t2));
    TEST_ASSERT_NOT_NULL(t2);
    TEST_ASSERT_NOT_EQUAL(t1, t2); /* Token sequence must have incremented */

    /* Stale t1 cannot operate on new timer t2 (Anti-ABA) */
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, gptimer_start(t1));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, gptimer_del_timer(t1));

    /* Valid t2 operations succeed */
    TEST_ASSERT_EQUAL_INT32(ESP_OK, gptimer_enable(t2));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, gptimer_start(t2));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, gptimer_stop(t2));
    TEST_ASSERT_EQUAL_INT32(ESP_OK, gptimer_del_timer(t2));

    /* Wild pointers must be rejected */
    gptimer_handle_t wild = (gptimer_handle_t)(uintptr_t)0xdeadbeef;
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, gptimer_start(wild));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_INVALID_ARG, gptimer_del_timer(wild));
}

void test_gptimer_cross_instance_sequence_handover(void) {
    uint32_t seq1 = esp_sim_handle_get_sequence();
    TEST_ASSERT_TRUE(seq1 > 0);

    /* Simulate another instance inheriting state */
    esp_sim_handle_set_sequence_base(seq1 + 100);
    uint32_t seq2 = esp_sim_handle_get_sequence();
    TEST_ASSERT_EQUAL_UINT32(seq1 + 100, seq2);

    /* Allocate timer, verify it takes the new sequence */
    gptimer_config_t cfg = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000
    };
    gptimer_handle_t t = NULL;
    TEST_ASSERT_EQUAL_INT32(ESP_OK, gptimer_new_timer(&cfg, &t));
    TEST_ASSERT_NOT_NULL(t);

    uint32_t token = (uint32_t)(uintptr_t)t;
    uint32_t token_seq = token >> 11;
    TEST_ASSERT_EQUAL_UINT32(seq1 + 101, token_seq);

    TEST_ASSERT_EQUAL_INT32(ESP_OK, gptimer_del_timer(t));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_gptimer_lifecycle_and_validation);
    RUN_TEST(test_gptimer_raw_count);
    RUN_TEST(test_gptimer_alarm_and_callback);
    RUN_TEST(test_gptimer_pool_limit);
    RUN_TEST(test_gptimer_stale_handle_and_aba);
    RUN_TEST(test_gptimer_cross_instance_sequence_handover);
    return UNITY_END();
}

