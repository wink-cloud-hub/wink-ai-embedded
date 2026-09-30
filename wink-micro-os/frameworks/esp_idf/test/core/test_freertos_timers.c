/* SPDX-License-Identifier: GPL-3.0-only */
#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/task.h"
#include "freertos_sync.h"
#include "esp_timer.h"
#include "esp_idf_wink.h"
#include "wink_sim_scheduler.h"

static int s_timer_cb_count = 0;
static TimerHandle_t s_last_timer_handle = NULL;
static uint32_t s_work_item_received_token = 0;
static int s_work_item_call_count = 0;

static void common_timer_cb(TimerHandle_t xTimer) {
    s_timer_cb_count++;
    s_last_timer_handle = xTimer;
}

static void common_work_item_fn(void *arg, uint32_t token) {
    (void)arg;
    s_work_item_call_count++;
    s_work_item_received_token = token;
}

void setUp(void) {
    sim_scheduler_reset(42);
    esp_freertos_pools_reset();
    s_timer_cb_count = 0;
    s_last_timer_handle = NULL;
    s_work_item_received_token = 0;
    s_work_item_call_count = 0;
}

void tearDown(void) {
    esp_freertos_pools_reset();
    sim_scheduler_reset(0);
}

/* --------------------------------------------------------------------------
 * 1. Timer Creation & Basic Attributes
 * -------------------------------------------------------------------------- */
void test_timer_create_and_validation(void) {
    /* Invalid arguments */
    TEST_ASSERT_NULL(xTimerCreate("invalid", 0, pdFALSE, NULL, common_timer_cb));
    TEST_ASSERT_NULL(xTimerCreate("invalid", 10, pdFALSE, NULL, NULL));

    /* Valid creation */
    void *my_id = (void *)(uintptr_t)0x1234;
    TimerHandle_t t = xTimerCreate("t1", 5, pdFALSE, my_id, common_timer_cb);
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT_EQUAL_PTR(my_id, pvTimerGetTimerID(t));
    TEST_ASSERT_EQUAL(pdFALSE, xTimerIsTimerActive(t));

    /* ISR functions should return pdFAIL */
    BaseType_t woken = pdFALSE;
    TEST_ASSERT_EQUAL(pdFAIL, xTimerStartFromISR(t, &woken));
    TEST_ASSERT_EQUAL(pdFALSE, woken);
    TEST_ASSERT_EQUAL(pdFAIL, xTimerStopFromISR(t, &woken));
    TEST_ASSERT_EQUAL(pdFAIL, xTimerResetFromISR(t, &woken));
    TEST_ASSERT_EQUAL(pdFAIL, xTimerChangePeriodFromISR(t, 10, &woken));

    xTimerDelete(t, 0);
}

/* --------------------------------------------------------------------------
 * 2. Oneshot Timer Expiration
 * -------------------------------------------------------------------------- */
static void oneshot_runner_task(void *arg) {
    (void)arg;
    TimerHandle_t t = xTimerCreate("oneshot", 3, pdFALSE, NULL, common_timer_cb);
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT_EQUAL(pdPASS, xTimerStart(t, 0));

    /* Delay 6 ticks; timer period is 3 ticks */
    vTaskDelay(6);

    /* Oneshot timer should have fired once and became inactive */
    TEST_ASSERT_EQUAL(1, s_timer_cb_count);
    TEST_ASSERT_EQUAL_PTR(t, s_last_timer_handle);
    TEST_ASSERT_EQUAL(pdFALSE, xTimerIsTimerActive(t));

    xTimerDelete(t, 0);
    vTaskDelete(NULL);
}

void test_timer_oneshot_expiration(void) {
    TaskHandle_t th;
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(oneshot_runner_task, "oneshot_task", 32768, NULL, 5, &th));
    wink_status_t st = pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 50);
    TEST_ASSERT_EQUAL(WINK_OK, st);
    TEST_ASSERT_EQUAL(1, s_timer_cb_count);
}

/* --------------------------------------------------------------------------
 * 3. Auto-Reload Timer Expiration & Stopping
 * -------------------------------------------------------------------------- */
static void autoreload_runner_task(void *arg) {
    (void)arg;
    TimerHandle_t t = xTimerCreate("autoreload", 2, pdTRUE, NULL, common_timer_cb);
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT_EQUAL(pdPASS, xTimerStart(t, 0));

    /* Delay 7 ticks; should fire at ticks 2, 4, 6 (3 times) */
    vTaskDelay(7);
    TEST_ASSERT_EQUAL(3, s_timer_cb_count);
    TEST_ASSERT_EQUAL(pdTRUE, xTimerIsTimerActive(t));

    /* Stop the timer and delay more; count should not increase */
    TEST_ASSERT_EQUAL(pdPASS, xTimerStop(t, 0));
    vTaskDelay(5);
    TEST_ASSERT_EQUAL(3, s_timer_cb_count);
    TEST_ASSERT_EQUAL(pdFALSE, xTimerIsTimerActive(t));

    xTimerDelete(t, 0);
    vTaskDelete(NULL);
}

void test_timer_auto_reload_expiration(void) {
    TaskHandle_t th;
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(autoreload_runner_task, "auto_task", 32768, NULL, 5, &th));
    wink_status_t st = pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 50);
    TEST_ASSERT_EQUAL(WINK_OK, st);
    TEST_ASSERT_EQUAL(3, s_timer_cb_count);
}

/* --------------------------------------------------------------------------
 * 4. Change Period & Reset
 * -------------------------------------------------------------------------- */
static void change_period_runner_task(void *arg) {
    (void)arg;
    /* Start with a 20-tick period */
    TimerHandle_t t = xTimerCreate("chg_p", 20, pdFALSE, NULL, common_timer_cb);
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT_EQUAL(pdPASS, xTimerStart(t, 0));

    /* At tick 2, change period to 3 ticks */
    vTaskDelay(2);
    TEST_ASSERT_EQUAL(0, s_timer_cb_count);
    TEST_ASSERT_EQUAL(pdPASS, xTimerChangePeriod(t, 3, 0));

    /* Wait 4 ticks (total tick 6); should fire at tick 2+3=5 */
    vTaskDelay(4);
    TEST_ASSERT_EQUAL(1, s_timer_cb_count);

    xTimerDelete(t, 0);
    vTaskDelete(NULL);
}

void test_timer_change_period(void) {
    TaskHandle_t th;
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(change_period_runner_task, "chg_task", 32768, NULL, 5, &th));
    wink_status_t st = pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 50);
    TEST_ASSERT_EQUAL(WINK_OK, st);
    TEST_ASSERT_EQUAL(1, s_timer_cb_count);
}

static void reset_runner_task(void *arg) {
    (void)arg;
    TimerHandle_t t = xTimerCreate("reset_t", 5, pdFALSE, NULL, common_timer_cb);
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT_EQUAL(pdPASS, xTimerStart(t, 0));

    /* At tick 3, reset the timer (deadline pushed from tick 5 to tick 3+5=8) */
    vTaskDelay(3);
    TEST_ASSERT_EQUAL(0, s_timer_cb_count);
    TEST_ASSERT_EQUAL(pdPASS, xTimerReset(t, 0));

    /* Wait 3 more ticks (now tick 6); original tick 5 passed but timer shouldn't fire yet */
    vTaskDelay(3);
    TEST_ASSERT_EQUAL(0, s_timer_cb_count);

    /* Wait 3 more ticks (now tick 9); reset deadline was tick 8, so it must fire */
    vTaskDelay(3);
    TEST_ASSERT_EQUAL(1, s_timer_cb_count);

    xTimerDelete(t, 0);
    vTaskDelete(NULL);
}

void test_timer_reset(void) {
    TaskHandle_t th;
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(reset_runner_task, "reset_task", 32768, NULL, 5, &th));
    wink_status_t st = pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 50);
    TEST_ASSERT_EQUAL(WINK_OK, st);
    TEST_ASSERT_EQUAL(1, s_timer_cb_count);
}

/* --------------------------------------------------------------------------
 * 5. P0 Critical Test: Early Wakeup on Insert (Timer Daemon Waiting Model)
 * -------------------------------------------------------------------------- */
static int s_early_timer_fired = 0;
static int s_long_timer_fired = 0;

static void early_timer_cb(TimerHandle_t xTimer) {
    (void)xTimer;
    s_early_timer_fired++;
}

static void long_timer_cb(TimerHandle_t xTimer) {
    (void)xTimer;
    s_long_timer_fired++;
}

static void early_wake_insert_task(void *arg) {
    (void)arg;
    /* 1. Start a long timer (100 ticks = 1000ms). Daemon will wait up to 100 ticks */
    TimerHandle_t t_long = xTimerCreate("long_t", 100, pdFALSE, NULL, long_timer_cb);
    TEST_ASSERT_NOT_NULL(t_long);
    TEST_ASSERT_EQUAL(pdPASS, xTimerStart(t_long, 0));

    /* 2. Delay 1 tick so Daemon enters xQueueReceive(..., wait_ticks = ~99) */
    vTaskDelay(1);
    TEST_ASSERT_EQUAL(0, s_long_timer_fired);

    /* 3. Insert a short timer with period = 2 ticks (20ms).
     * If Daemon were in vTaskDelay, it would ignore this until tick 100!
     * With xQueueReceive, xQueueSend resumes the Daemon fiber immediately! */
    TimerHandle_t t_short = xTimerCreate("short_t", 2, pdFALSE, NULL, early_timer_cb);
    TEST_ASSERT_NOT_NULL(t_short);
    TEST_ASSERT_EQUAL(pdPASS, xTimerStart(t_short, 0));

    /* 4. Delay 4 ticks (total tick ~5). Short timer must fire at tick 1+2=3! */
    vTaskDelay(4);
    TEST_ASSERT_EQUAL(1, s_early_timer_fired);
    TEST_ASSERT_EQUAL(0, s_long_timer_fired);

    xTimerDelete(t_short, 0);
    xTimerDelete(t_long, 0);
    vTaskDelete(NULL);
}

void test_freertos_timers_early_wake_insert(void) {
    s_early_timer_fired = 0;
    s_long_timer_fired = 0;

    TaskHandle_t th;
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(early_wake_insert_task, "early_task", 32768, NULL, 5, &th));
    wink_status_t st = pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 50);
    TEST_ASSERT_EQUAL(WINK_OK, st);
    TEST_ASSERT_EQUAL(1, s_early_timer_fired);
    TEST_ASSERT_EQUAL(0, s_long_timer_fired);
}

/* --------------------------------------------------------------------------
 * 6. P1 Critical Test: Slot Reuse Anti-ABA Defense
 * -------------------------------------------------------------------------- */
static int s_timer_a_calls = 0;
static int s_timer_b_calls = 0;

static void timer_a_cb(TimerHandle_t xTimer) {
    (void)xTimer;
    s_timer_a_calls++;
}

static void timer_b_cb(TimerHandle_t xTimer) {
    (void)xTimer;
    s_timer_b_calls++;
}

static void slot_reuse_aba_task(void *arg) {
    (void)arg;
    /* Create Timer A, start it with period 10 */
    TimerHandle_t tA = xTimerCreate("timerA", 10, pdFALSE, NULL, timer_a_cb);
    TEST_ASSERT_NOT_NULL(tA);
    TEST_ASSERT_EQUAL(pdPASS, xTimerStart(tA, 0));

    /* Immediately delete Timer A */
    TEST_ASSERT_EQUAL(pdPASS, xTimerDelete(tA, 0));

    /* Immediately create Timer B, which reuses the same slot but gets a new generation */
    TimerHandle_t tB = xTimerCreate("timerB", 10, pdFALSE, NULL, timer_b_cb);
    TEST_ASSERT_NOT_NULL(tB);
    TEST_ASSERT_NOT_EQUAL(tA, tB); /* Token sequence/generation must differ! */

    /* Run 15 ticks without starting Timer B */
    vTaskDelay(15);

    /* Neither Timer A nor Timer B should have fired */
    TEST_ASSERT_EQUAL(0, s_timer_a_calls);
    TEST_ASSERT_EQUAL(0, s_timer_b_calls);

    xTimerDelete(tB, 0);
    vTaskDelete(NULL);
}

void test_freertos_timers_slot_reuse_aba(void) {
    s_timer_a_calls = 0;
    s_timer_b_calls = 0;

    TaskHandle_t th;
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(slot_reuse_aba_task, "aba_task", 32768, NULL, 5, &th));
    wink_status_t st = pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 50);
    TEST_ASSERT_EQUAL(WINK_OK, st);
    TEST_ASSERT_EQUAL(0, s_timer_a_calls);
    TEST_ASSERT_EQUAL(0, s_timer_b_calls);
}

/* --------------------------------------------------------------------------
 * 7. Command Queue Backpressure & Non-blocking Fail-Loud
 * -------------------------------------------------------------------------- */
void test_freertos_timers_queue_full_backpressure(void) {
    TimerHandle_t t = xTimerCreate("bp_t", 50, pdFALSE, (void *)(uintptr_t)0x55aa, common_timer_cb);
    TEST_ASSERT_NOT_NULL(t);

    /* Fill the command queue (length = 8) */
    for (int i = 0; i < 8; i++) {
        TEST_ASSERT_EQUAL(pdPASS, xTimerStart(t, 0));
    }

    /* 9th command with xTicksToWait = 0 must fail with pdFAIL */
    TEST_ASSERT_EQUAL(pdFAIL, xTimerStart(t, 0));

    /* Queue is full: stop, change period, delete must fail and safely rollback without handle destruction */
    TEST_ASSERT_EQUAL(pdFAIL, xTimerStop(t, 0));
    TEST_ASSERT_EQUAL(pdFAIL, xTimerChangePeriod(t, 100, 0));
    TEST_ASSERT_EQUAL(pdFAIL, xTimerDelete(t, 0));

    /* Verify handle is still intact and not prematurely destroyed by failed delete */
    TEST_ASSERT_EQUAL_PTR((void *)(uintptr_t)0x55aa, pvTimerGetTimerID(t));

    /* Work item post must fail gracefully and not leak slot when queue is full */
    uint32_t work_tok = 999;
    TEST_ASSERT_EQUAL(pdFAIL, esp_freertos_timer_post_work_item(common_work_item_fn, NULL, &work_tok, 0));
    TEST_ASSERT_EQUAL(0, work_tok);

    /* Run scheduler to drain queue */
    wink_status_t st = pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 10);
    TEST_ASSERT_EQUAL(WINK_OK, st);

    /* Now that queue is drained, delete must succeed */
    TEST_ASSERT_EQUAL(pdPASS, xTimerDelete(t, 0));

    /* Run scheduler to process delete command */
    pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 5);

    /* Second delete on retired timer handle must fail */
    TEST_ASSERT_EQUAL(pdFAIL, xTimerDelete(t, 0));
}

/* --------------------------------------------------------------------------
 * 8. Reset DAG: In-Flight Expiry Hard Reset Zero Ghost Callback
 * -------------------------------------------------------------------------- */
static int s_ghost_cb_called = 0;

static void ghost_timer_cb(TimerHandle_t xTimer) {
    (void)xTimer;
    s_ghost_cb_called++;
}

static void in_flight_restart_task(void *arg) {
    (void)arg;
    /* Create and start a timer that will expire at tick 10 */
    TimerHandle_t t = xTimerCreate("ghost_t", 10, pdFALSE, NULL, ghost_timer_cb);
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT_EQUAL(pdPASS, xTimerStart(t, 0));

    /* Delay 8 ticks (2 ticks before expiration) */
    vTaskDelay(8);
    TEST_ASSERT_EQUAL(0, s_ghost_cb_called);

    /* Simulate soft reset at Stage 5.5 */
    esp_freertos_timers_sim_reset();
    TEST_ASSERT_EQUAL(0, esp_freertos_get_active_timer_count());

    /* Advance 10 more ticks past the original tick 10 mark */
    vTaskDelay(10);

    /* Callback must NEVER be called */
    TEST_ASSERT_EQUAL(0, s_ghost_cb_called);
    vTaskDelete(NULL);
}

void test_freertos_timers_in_flight_restart_no_ghost_callback(void) {
    s_ghost_cb_called = 0;
    TaskHandle_t th;
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(in_flight_restart_task, "restart_task", 32768, NULL, 5, &th));
    wink_status_t st = pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 50);
    TEST_ASSERT_EQUAL(WINK_OK, st);
    TEST_ASSERT_EQUAL(0, s_ghost_cb_called);
}

/* --------------------------------------------------------------------------
 * 9. Work Item Extension (Wi-Fi / MQTT delayed connect work)
 * -------------------------------------------------------------------------- */
static void work_item_runner_task(void *arg) {
    (void)arg;
    uint32_t token1 = 0;
    uint32_t token2 = 0;

    /* Post work item 1 with delay 3 ticks */
    TEST_ASSERT_EQUAL(pdPASS, esp_freertos_timer_post_work_item(common_work_item_fn, NULL, &token1, 3));
    TEST_ASSERT_NOT_EQUAL(0, token1);

    /* Post work item 2 with delay 5 ticks and immediately cancel it */
    TEST_ASSERT_EQUAL(pdPASS, esp_freertos_timer_post_work_item(common_work_item_fn, NULL, &token2, 5));
    TEST_ASSERT_NOT_EQUAL(0, token2);
    TEST_ASSERT_EQUAL(pdPASS, esp_freertos_timer_cancel_work_item(token2));

    /* Delay 6 ticks */
    vTaskDelay(6);

    /* Work item 1 should have fired; work item 2 was cancelled so never fired */
    TEST_ASSERT_EQUAL(1, s_work_item_call_count);
    TEST_ASSERT_EQUAL(token1, s_work_item_received_token);

    vTaskDelete(NULL);
}

void test_timer_work_item_execution_and_cancel(void) {
    TaskHandle_t th;
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(work_item_runner_task, "work_task", 32768, NULL, 5, &th));
    wink_status_t st = pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 50);
    TEST_ASSERT_EQUAL(WINK_OK, st);
    TEST_ASSERT_EQUAL(1, s_work_item_call_count);
}

/* --------------------------------------------------------------------------
 * 10. esp_timer Fail-Loud Contract (ADR-0012)
 * -------------------------------------------------------------------------- */
void test_esp_timer_fail_loud(void) {
    esp_timer_handle_t th = NULL;
    esp_timer_create_args_t args = {
        .callback = NULL,
        .arg = NULL,
        .name = "unsupported"
    };

    TEST_ASSERT_EQUAL_INT32(ESP_ERR_NOT_SUPPORTED, esp_timer_create(&args, &th));
    TEST_ASSERT_NULL(th);
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_NOT_SUPPORTED, esp_timer_start_once(th, 1000));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_NOT_SUPPORTED, esp_timer_start_periodic(th, 1000));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_NOT_SUPPORTED, esp_timer_stop(th));
    TEST_ASSERT_EQUAL_INT32(ESP_ERR_NOT_SUPPORTED, esp_timer_delete(th));

    /* esp_timer_get_time returns valid monotonically non-decreasing microseconds */
    int64_t t0 = esp_timer_get_time();
    TEST_ASSERT_GREATER_OR_EQUAL_INT64(0, t0);
}

/* --------------------------------------------------------------------------
 * Test Runner Main
 * -------------------------------------------------------------------------- */
int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_timer_create_and_validation);
    RUN_TEST(test_timer_oneshot_expiration);
    RUN_TEST(test_timer_auto_reload_expiration);
    RUN_TEST(test_timer_change_period);
    RUN_TEST(test_timer_reset);
    RUN_TEST(test_freertos_timers_early_wake_insert);
    RUN_TEST(test_freertos_timers_slot_reuse_aba);
    RUN_TEST(test_freertos_timers_queue_full_backpressure);
    RUN_TEST(test_freertos_timers_in_flight_restart_no_ghost_callback);
    RUN_TEST(test_timer_work_item_execution_and_cancel);
    RUN_TEST(test_esp_timer_fail_loud);

    return UNITY_END();
}
