// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_pal_mcpwm.c
 * @brief PAL MCPWM motor control PWM and complementary dead-time unit tests.
 */
#include "unity.h"
#include "hal/pal_mcpwm.h"
#include "pal_resource.h"
#include "pal_mcpwm_stub.h"

static bool s_brake_called = false;
static bool s_capture_called = false;
static uint32_t s_captured_ts = 0;

static void test_brake_isr_cb(void *arg) {
    (void)arg;
    s_brake_called = true;
}

static void test_capture_isr_cb(void *arg, uint32_t ts_ns, bool rising) {
    (void)arg;
    (void)rising;
    s_capture_called = true;
    s_captured_ts = ts_ns;
}

void setUp(void) {
    pal_resource_reset();
    s_brake_called = false;
    s_capture_called = false;
    s_captured_ts = 0;
}

void tearDown(void) {
    pal_resource_reset();
}

void test_mcpwm_timer_and_oper_init(void) {
    pal_mcpwm_timer_cfg_t t_cfg = {
        .mcpwm_unit = 0,
        .timer_id = 0,
        .pwm_freq_hz = 20000,
        .counter_top = 1000,
        .core_affinity = PAL_OS_CORE_1,
        .iram_safe = true,
    };

    pal_mcpwm_timer_handle_t timer = NULL;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_mcpwm_new_timer(&t_cfg, &timer));
    TEST_ASSERT_NOT_NULL(timer);

    pal_mcpwm_oper_cfg_t oper_cfg = {
        .timer = timer,
        .operator_id = 0,
        .pin_pwm_a = 18,
        .pin_pwm_b = 19,
        .deadtime_red_ticks = 50,
        .deadtime_fed_ticks = 50,
        .complementary_enable = true,
    };

    /* Verify zero deadtime rejection for complementary pairs (shoot-through prevention) */
    pal_mcpwm_oper_cfg_t zero_dt_cfg = oper_cfg;
    zero_dt_cfg.deadtime_red_ticks = 0;
    pal_mcpwm_oper_handle_t bad_oper = NULL;
    TEST_ASSERT_EQUAL_INT(WINK_ERR_INVALID_ARG, pal_mcpwm_new_oper(&zero_dt_cfg, &bad_oper));

    pal_mcpwm_oper_handle_t oper = NULL;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_mcpwm_new_oper(&oper_cfg, &oper));
    TEST_ASSERT_NOT_NULL(oper);

    pal_mcpwm_cmp_cfg_t cmp_cfg = {
        .oper = oper,
        .initial_duty_ticks = 500,
    };

    pal_mcpwm_cmp_handle_t cmp = NULL;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_mcpwm_new_cmp(&cmp_cfg, &cmp));
    TEST_ASSERT_NOT_NULL(cmp);
    TEST_ASSERT_EQUAL_UINT32(500, stub_mcpwm_get_duty_ticks(cmp));

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_mcpwm_set_duty_ticks(cmp, 750));
    TEST_ASSERT_EQUAL_UINT32(750, stub_mcpwm_get_duty_ticks(cmp));

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_mcpwm_timer_start(timer));
    TEST_ASSERT_TRUE(stub_mcpwm_is_timer_running(timer));

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_mcpwm_timer_stop(timer));
    TEST_ASSERT_FALSE(stub_mcpwm_is_timer_running(timer));

    pal_mcpwm_del_timer(timer);
}

void test_mcpwm_fault_and_brake(void) {
    pal_mcpwm_fault_cfg_t fault_cfg = {
        .fault_id = 0,
        .fault_pin = 21,
        .active_level = false,
        .async_brake = true,
        .safe_level_a = false,
        .safe_level_b = false,
        .on_brake_isr = test_brake_isr_cb,
    };

    pal_mcpwm_fault_handle_t fault = NULL;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_mcpwm_new_fault(&fault_cfg, &fault));
    TEST_ASSERT_NOT_NULL(fault);

    stub_mcpwm_trigger_brake(fault);
    TEST_ASSERT_TRUE(s_brake_called);

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_mcpwm_fault_clear(fault));
}

void test_mcpwm_capture(void) {
    pal_mcpwm_cap_cfg_t cap_cfg = {
        .cap_pin = 22,
        .cap_channel = 0,
        .pull_up = true,
        .on_capture_isr = test_capture_isr_cb,
    };

    pal_mcpwm_cap_handle_t cap = NULL;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_mcpwm_new_capture(&cap_cfg, &cap));
    TEST_ASSERT_NOT_NULL(cap);

    stub_mcpwm_trigger_capture(cap, 500000, true);
    TEST_ASSERT_TRUE(s_capture_called);
    TEST_ASSERT_EQUAL_UINT32(500000, s_captured_ts);
}

void test_mcpwm_sync_and_phase_lock(void) {
    pal_mcpwm_timer_cfg_t t0_cfg = {
        .mcpwm_unit = 0,
        .timer_id = 0,
        .pwm_freq_hz = 20000,
        .counter_top = 1000,
    };
    pal_mcpwm_timer_cfg_t t1_cfg = {
        .mcpwm_unit = 0,
        .timer_id = 1,
        .pwm_freq_hz = 20000,
        .counter_top = 1000,
    };

    pal_mcpwm_timer_handle_t t0 = NULL;
    pal_mcpwm_timer_handle_t t1 = NULL;
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_mcpwm_new_timer(&t0_cfg, &t0));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_mcpwm_new_timer(&t1_cfg, &t1));

    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_mcpwm_sync_gpio_config(23, true));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_mcpwm_timer_enable_phase_lock(t0, 0));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_mcpwm_timer_enable_phase_lock(t1, 333));
    TEST_ASSERT_EQUAL_INT(WINK_OK, pal_mcpwm_trigger_software_sync());

    pal_mcpwm_del_timer(t0);
    pal_mcpwm_del_timer(t1);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_mcpwm_timer_and_oper_init);
    RUN_TEST(test_mcpwm_fault_and_brake);
    RUN_TEST(test_mcpwm_capture);
    RUN_TEST(test_mcpwm_sync_and_phase_lock);
    return UNITY_END();
}
