/* SPDX-License-Identifier: GPL-3.0-only */
#include "unity.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "wink_app.h"
#include "wink_sim_scheduler.h"
#include "pal_osal.h"
#include "freertos_sync.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

extern const wink_app_callbacks_t* wink_app_get_callbacks(void);
extern void pal_host_reset_gpio_levels(void);
extern uint32_t esp_idf_get_app_main_task_id(void);
extern void sim_set_mono_time_us(uint64_t us);

void setUp(void) {
    sim_set_mono_time_us(0);
    sim_scheduler_reset(42);
    esp_freertos_pools_reset();
    pal_host_reset_gpio_levels();
}

void tearDown(void) {
    sim_set_mono_time_us(0);
    sim_scheduler_reset(0);
    esp_freertos_pools_reset();
    pal_host_reset_gpio_levels();
}

void test_esp_idf_blink_bounded_execution(void) {
    const wink_app_callbacks_t* cb = wink_app_get_callbacks();
    TEST_ASSERT_NOT_NULL(cb);
    TEST_ASSERT_NOT_NULL(cb->init);

    /* Initialize framework, which registers app_main as task fiber */
    cb->init();

    uint32_t main_task_id = esp_idf_get_app_main_task_id();
    TEST_ASSERT_NOT_EQUAL(SIM_SCHED_NO_READY, main_task_id);

    int levels[4];
    uint64_t times[4];
    int flip_count = 0;

    for (int step = 0; step < 4; ++step) {
        wink_status_t st = pal_sim_scheduler_run(cb, main_task_id, 1);
        TEST_ASSERT_EQUAL(WINK_OK, st);
        levels[step] = gpio_get_level((gpio_num_t)CONFIG_BLINK_GPIO);
        times[step] = pal_os_get_us();
        if (step > 0 && levels[step] != levels[step - 1]) {
            flip_count++;
        }
    }

    /* Assert that GPIO flipped at least 2 times */
    TEST_ASSERT_GREATER_OR_EQUAL(2, flip_count);
    /* In blink: step 0: level 0; step 1: level 1; step 2: level 0; step 3: level 1 */
    TEST_ASSERT_EQUAL(0, levels[0]);
    TEST_ASSERT_EQUAL(1, levels[1]);
    TEST_ASSERT_EQUAL(0, levels[2]);
    TEST_ASSERT_EQUAL(1, levels[3]);

    /* Assert time advances by approx 1,000,000 us (1 second) per step */
    TEST_ASSERT_GREATER_OR_EQUAL(1000000ULL, times[1] - times[0]);
    TEST_ASSERT_GREATER_OR_EQUAL(1000000ULL, times[2] - times[1]);
    TEST_ASSERT_GREATER_OR_EQUAL(1000000ULL, times[3] - times[2]);
}

void test_esp_idf_blink_replay_determinism(void) {
    const wink_app_callbacks_t* cb = wink_app_get_callbacks();
    TEST_ASSERT_NOT_NULL(cb);

    /* Run 1 */
    sim_set_mono_time_us(0);
    sim_scheduler_reset(42);
    esp_freertos_pools_reset();
    pal_host_reset_gpio_levels();
    cb->init();
    uint32_t main_task_id = esp_idf_get_app_main_task_id();

    int run1_levels[4];
    uint64_t run1_times[4];
    for (int step = 0; step < 4; ++step) {
        TEST_ASSERT_EQUAL(WINK_OK, pal_sim_scheduler_run(cb, main_task_id, 1));
        run1_levels[step] = gpio_get_level((gpio_num_t)CONFIG_BLINK_GPIO);
        run1_times[step] = pal_os_get_us();
    }

    /* Run 2 */
    sim_set_mono_time_us(0);
    sim_scheduler_reset(42);
    esp_freertos_pools_reset();
    pal_host_reset_gpio_levels();
    cb->init();
    main_task_id = esp_idf_get_app_main_task_id();

    int run2_levels[4];
    uint64_t run2_times[4];
    for (int step = 0; step < 4; ++step) {
        TEST_ASSERT_EQUAL(WINK_OK, pal_sim_scheduler_run(cb, main_task_id, 1));
        run2_levels[step] = gpio_get_level((gpio_num_t)CONFIG_BLINK_GPIO);
        run2_times[step] = pal_os_get_us();
    }

    /* Assert byte-for-byte exact equality between run 1 and run 2 */
    TEST_ASSERT_EQUAL_MEMORY(run1_levels, run2_levels, sizeof(run1_levels));
    TEST_ASSERT_EQUAL_MEMORY(run1_times, run2_times, sizeof(run1_times));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_esp_idf_blink_bounded_execution);
    RUN_TEST(test_esp_idf_blink_replay_determinism);
    return UNITY_END();
}
