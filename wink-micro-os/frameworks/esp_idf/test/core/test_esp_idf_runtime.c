/* SPDX-License-Identifier: GPL-3.0-only */
#include "unity.h"
#include "wink_app.h"
#include "esp_idf_wink.h"
#include "wink_sim_scheduler.h"
#include "esp_system.h"
#include <stdint.h>

/* Defined (weak) in src/esp_idf_runtime.c; strong app_main comes from the app/corpus. */
extern void app_main(void);
extern uint32_t esp_idf_get_app_main_task_id(void);
extern const wink_app_callbacks_t *wink_app_get_callbacks(void);
extern bool pal_wasm_target_has_pending_reset(void);
extern void pal_wasm_target_clear_pending_reset(void);

static bool s_restart_fiber_continued;
static bool s_scheduler_sentinel_ran;

static void restart_fiber(void *arg) {
    (void)arg;
    esp_restart();
    s_restart_fiber_continued = true;
}

static void scheduler_sentinel(void *arg) {
    (void)arg;
    s_scheduler_sentinel_ran = true;
}

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

void test_esp_restart_fiber_does_not_resume_user_code(void) {
    s_restart_fiber_continued = false;
    s_scheduler_sentinel_ran = false;
    uint32_t restart_id = SIM_SCHED_NO_READY;
    uint32_t sentinel_id = SIM_SCHED_NO_READY;
    TEST_ASSERT_EQUAL(WINK_OK, sim_scheduler_register(
        restart_fiber, NULL, "restart_fiber", 1, 0, 32768, &restart_id));
    TEST_ASSERT_EQUAL(WINK_OK, sim_scheduler_register(
        scheduler_sentinel, NULL, "restart_sentinel", 1, 0, 32768, &sentinel_id));
    TEST_ASSERT_NOT_EQUAL(SIM_SCHED_NO_READY, restart_id);
    TEST_ASSERT_NOT_EQUAL(SIM_SCHED_NO_READY, sentinel_id);

    TEST_ASSERT_EQUAL(WINK_OK, pal_sim_scheduler_run(NULL, sentinel_id, 10));
    TEST_ASSERT_TRUE(s_scheduler_sentinel_ran);
    TEST_ASSERT_FALSE(s_restart_fiber_continued);
    TEST_ASSERT_TRUE(pal_wasm_target_has_pending_reset());

    /* Apply reset only after the scheduler returned to its main context. */
    pal_wasm_target_clear_pending_reset();
    TEST_ASSERT_FALSE(pal_wasm_target_has_pending_reset());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_weak_app_main_default_is_callable);
    RUN_TEST(test_callbacks_init_registers_app_main_fiber);
    RUN_TEST(test_esp_restart_fiber_does_not_resume_user_code);
    return UNITY_END();
}
