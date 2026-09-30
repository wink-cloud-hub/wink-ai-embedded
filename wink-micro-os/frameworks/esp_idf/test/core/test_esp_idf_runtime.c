/* SPDX-License-Identifier: GPL-3.0-only */
#include "unity.h"
#include "wink_app.h"
#include "esp_idf_wink.h"
#include "wink_sim_scheduler.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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

static void graceful_dummy_task(void *arg) {
    (void)arg;
    vTaskDelete(NULL);
}

void test_dual_assertion_mode_a_graceful_shutdown(void) {
    size_t alloc_base = esp_heap_caps_get_active_allocations();
    uint32_t task_base = esp_freertos_get_active_task_count();

    void *ptr1 = heap_caps_malloc(64, MALLOC_CAP_DMA);
    void *ptr2 = heap_caps_malloc(128, MALLOC_CAP_DMA);
    TEST_ASSERT_NOT_NULL(ptr1);
    TEST_ASSERT_NOT_NULL(ptr2);
    TEST_ASSERT_EQUAL_UINT32(alloc_base + 2, esp_heap_caps_get_active_allocations());

    TaskHandle_t th = NULL;
    BaseType_t r = xTaskCreate(graceful_dummy_task, "grace_task", 1024, NULL, 1, &th);
    TEST_ASSERT_EQUAL_INT(pdPASS, r);
    TEST_ASSERT_EQUAL_UINT32(task_base + 1, esp_freertos_get_active_task_count());

    vTaskDelete(th);
    heap_caps_free(ptr1);
    heap_caps_free(ptr2);

    /* Delta must be exactly 0 (graceful shutdown zero leakage assertion) */
    TEST_ASSERT_EQUAL_UINT32(alloc_base, esp_heap_caps_get_active_allocations());
    TEST_ASSERT_EQUAL_UINT32(task_base, esp_freertos_get_active_task_count());
}

static void dummy_loop_task(void *arg) {
    (void)arg;
    while (1) {
        vTaskDelay(10);
    }
}

void test_dual_assertion_mode_b_inflight_restart_clean_baseline(void) {
    void *leaked_buf = heap_caps_malloc(256, MALLOC_CAP_DMA);
    TEST_ASSERT_NOT_NULL(leaked_buf);

    TaskHandle_t th1 = NULL;
    TaskHandle_t th2 = NULL;
    TEST_ASSERT_EQUAL_INT(pdPASS, xTaskCreate(dummy_loop_task, "loop1", 1024, NULL, 1, &th1));
    TEST_ASSERT_EQUAL_INT(pdPASS, xTaskCreate(dummy_loop_task, "loop2", 1024, NULL, 1, &th2));

    TEST_ASSERT_TRUE(esp_heap_caps_get_active_allocations() > 0);
    TEST_ASSERT_TRUE(esp_freertos_get_active_task_count() >= 2);

    /* Trigger soft reset via pal_wasm_target_clear_pending_reset() */
    pal_wasm_target_clear_pending_reset();

    /* Post-reset baseline check: task count and active allocations must reset to 0 */
    TEST_ASSERT_EQUAL_UINT32(0, esp_freertos_get_active_task_count());
    TEST_ASSERT_EQUAL_UINT32(0, esp_heap_caps_get_active_allocations());

    /* Verify stale task handles cannot be operated on */
    TEST_ASSERT_EQUAL_INT(eDeleted, eTaskGetState(th1));
    TEST_ASSERT_EQUAL_INT(eDeleted, eTaskGetState(th2));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_weak_app_main_default_is_callable);
    RUN_TEST(test_callbacks_init_registers_app_main_fiber);
    RUN_TEST(test_esp_restart_fiber_does_not_resume_user_code);
    RUN_TEST(test_dual_assertion_mode_a_graceful_shutdown);
    RUN_TEST(test_dual_assertion_mode_b_inflight_restart_clean_baseline);
    return UNITY_END();
}
