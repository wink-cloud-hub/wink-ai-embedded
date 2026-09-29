/* SPDX-License-Identifier: GPL-3.0-only */
#include "unity.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "wink_app.h"
#include "wink_sim_scheduler.h"
#include "pal_osal.h"
#include "freertos_sync.h"
#include "driver/ledc.h"
#include "host_test_ctrl.h"

extern const wink_app_callbacks_t* wink_app_get_callbacks(void);
extern uint32_t esp_idf_get_app_main_task_id(void);
extern void esp_ledc_reset(void);

void setUp(void) {
    sim_set_mono_time_us(0);
    sim_scheduler_reset(42);
    esp_freertos_pools_reset();
    sim_reset_time();
    pal_host_reset_gpio_levels();
}

void tearDown(void) {
    sim_set_mono_time_us(0);
    sim_scheduler_reset(0);
    esp_freertos_pools_reset();
    sim_reset_time();
    pal_host_reset_gpio_levels();
}

void test_esp_idf_ledc_bounded_execution(void) {
    const wink_app_callbacks_t* cb = wink_app_get_callbacks();
    TEST_ASSERT_NOT_NULL(cb);
    TEST_ASSERT_NOT_NULL(cb->init);

    /* Initialize framework, which registers app_main as task fiber */
    cb->init();

    uint32_t main_task_id = esp_idf_get_app_main_task_id();
    TEST_ASSERT_NOT_EQUAL(SIM_SCHED_NO_READY, main_task_id);

    /* Run 1 scheduler step to allow app_main to execute */
    wink_status_t st = pal_sim_scheduler_run(cb, main_task_id, 1);
    TEST_ASSERT_EQUAL(WINK_OK, st);

    /* In ledc_basic corpus example:
     * Mode: LOW_SPEED_MODE, Channel: 0, Output IO: 2, Resolution: 13-bit, Duty: 4096 (50%)
     * Verify that PWM channel 0 recorded duty is 50.0% (5000 bp / 100) */
    float duty = sim_last_pwm_duty(0);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.0f, duty);
}

void test_esp_idf_ledc_replay_determinism(void) {
    const wink_app_callbacks_t* cb = wink_app_get_callbacks();
    TEST_ASSERT_NOT_NULL(cb);

    /* Run 1 */
    sim_set_mono_time_us(0);
    sim_scheduler_reset(42);
    esp_freertos_pools_reset();
    sim_reset_time();
    pal_host_reset_gpio_levels();
    cb->init();
    uint32_t main_task_id = esp_idf_get_app_main_task_id();

    TEST_ASSERT_EQUAL(WINK_OK, pal_sim_scheduler_run(cb, main_task_id, 1));
    float run1_duty = sim_last_pwm_duty(0);
    uint64_t run1_time = pal_os_get_us();

    /* Run 2 */
    sim_set_mono_time_us(0);
    sim_scheduler_reset(42);
    esp_freertos_pools_reset();
    sim_reset_time();
    pal_host_reset_gpio_levels();
    cb->init();
    main_task_id = esp_idf_get_app_main_task_id();

    TEST_ASSERT_EQUAL(WINK_OK, pal_sim_scheduler_run(cb, main_task_id, 1));
    float run2_duty = sim_last_pwm_duty(0);
    uint64_t run2_time = pal_os_get_us();

    /* Assert bit-exact deterministic parity */
    TEST_ASSERT_EQUAL_FLOAT(run1_duty, run2_duty);
    TEST_ASSERT_EQUAL_UINT64(run1_time, run2_time);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_esp_idf_ledc_bounded_execution);
    RUN_TEST(test_esp_idf_ledc_replay_determinism);
    return UNITY_END();
}
