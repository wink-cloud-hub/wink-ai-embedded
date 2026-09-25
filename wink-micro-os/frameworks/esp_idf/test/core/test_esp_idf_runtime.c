/* SPDX-License-Identifier: GPL-3.0-only */
#include "unity.h"
#include "wink_app.h"
#include "esp_idf_wink.h"
#include "wink_sim_scheduler.h"
#include <stdint.h>

/* Defined (weak) in src/esp_idf_runtime.c; strong app_main comes from the app/corpus. */
extern void app_main(void);
extern uint32_t esp_idf_get_app_main_task_id(void);
extern const wink_app_callbacks_t *wink_app_get_callbacks(void);

void setUp(void) {
    sim_scheduler_reset(21);
    esp_freertos_pools_reset();
}

void tearDown(void) {
    sim_scheduler_reset(0);
    esp_freertos_pools_reset();
}

void test_weak_app_main_default_is_callable(void) {
    app_main(); /* weak default no-op must not crash */
}

void test_callbacks_init_registers_app_main_fiber(void) {
    const wink_app_callbacks_t *cb = wink_app_get_callbacks();
    TEST_ASSERT_NOT_NULL(cb);
    TEST_ASSERT_NOT_NULL(cb->init);
    TEST_ASSERT_NOT_NULL(cb->loop);
    TEST_ASSERT_NULL(cb->on_fault);

    cb->init();
    TEST_ASSERT_NOT_EQUAL(UINT32_MAX, esp_idf_get_app_main_task_id());

    /* Run the scheduler: the app_main trampoline executes and self-zombies */
    TEST_ASSERT_EQUAL(WINK_OK, pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 10));

    cb->loop(); /* no-op per ADR-0070 */
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_weak_app_main_default_is_callable);
    RUN_TEST(test_callbacks_init_registers_app_main_fiber);
    return UNITY_END();
}
