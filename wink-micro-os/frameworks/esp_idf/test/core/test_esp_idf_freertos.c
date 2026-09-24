/* SPDX-License-Identifier: GPL-3.0-only */
#include "unity.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "freertos_sync.h"
#include "wink_sim_scheduler.h"
#include "pal_osal.h"

extern void sim_set_mono_time_us(uint64_t us);

void setUp(void) {
    sim_set_mono_time_us(0);
    sim_scheduler_reset(42);
    esp_freertos_pools_reset();
}

void tearDown(void) {
    sim_set_mono_time_us(0);
    sim_scheduler_reset(0);
    esp_freertos_pools_reset();
}

/* --------------------------------------------------------------------------
 * 1. ABA Handle Protection
 * -------------------------------------------------------------------------- */
static void dummy_task_fn(void* arg) {
    (void)arg;
    for (;;) {
        vTaskDelay(10);
    }
}

void test_task_handle_aba_protection(void) {
    TaskHandle_t hA = NULL;
    TaskHandle_t hB = NULL;

    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(dummy_task_fn, "taskA", 32 * 1024, NULL, 5, &hA));
    TEST_ASSERT_NOT_NULL(hA);
    TEST_ASSERT_EQUAL(5, uxTaskPriorityGet(hA));

    vTaskDelete(hA);
    TEST_ASSERT_EQUAL(eDeleted, eTaskGetState(hA));
    TEST_ASSERT_EQUAL(0, uxTaskPriorityGet(hA));

    /* Create task B, which reuses the same slot */
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(dummy_task_fn, "taskB", 32 * 1024, NULL, 8, &hB));
    TEST_ASSERT_NOT_NULL(hB);
    TEST_ASSERT_NOT_EQUAL(hA, hB); /* Generation counter must differ! */
    TEST_ASSERT_EQUAL(8, uxTaskPriorityGet(hB));

    /* Operating on stale handle hA must fail and not affect hB */
    vTaskPrioritySet(hA, 1);
    TEST_ASSERT_EQUAL(8, uxTaskPriorityGet(hB));
    TEST_ASSERT_EQUAL(0, uxTaskPriorityGet(hA));

    vTaskDelete(hB);
}

/* --------------------------------------------------------------------------
 * 2. NULL Handle Resolution & 3. Self Deletion
 * -------------------------------------------------------------------------- */
static bool s_self_delete_reached_after = false;
static uint32_t s_null_handle_prio_read = 0;

static void self_ops_task(void* arg) {
    (void)arg;
    vTaskPrioritySet(NULL, 12);
    s_null_handle_prio_read = uxTaskPriorityGet(NULL);

    vTaskDelete(NULL);
    s_self_delete_reached_after = true; /* Must never be reached! */
}

void test_task_null_handle_and_self_delete(void) {
    s_self_delete_reached_after = false;
    s_null_handle_prio_read = 0;

    TaskHandle_t h = NULL;
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(self_ops_task, "self_ops", 32 * 1024, NULL, 3, &h));

    wink_status_t st = pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 50);
    TEST_ASSERT_EQUAL(WINK_OK, st);

    TEST_ASSERT_EQUAL(12, s_null_handle_prio_read);
    TEST_ASSERT_FALSE(s_self_delete_reached_after);
    TEST_ASSERT_EQUAL(eDeleted, eTaskGetState(h));
}

/* --------------------------------------------------------------------------
 * 4. Delay(0) Pure Yield Order (Round-Robin with same priority)
 * -------------------------------------------------------------------------- */
static int s_yield_trace[4];
static int s_yield_trace_idx = 0;

static void yield_task1(void* arg) {
    (void)arg;
    s_yield_trace[s_yield_trace_idx++] = 1;
    vTaskDelay(0); /* Yields context back to main; remains READY */
    s_yield_trace[s_yield_trace_idx++] = 3;
    vTaskDelete(NULL);
}

static void yield_task2(void* arg) {
    (void)arg;
    s_yield_trace[s_yield_trace_idx++] = 2;
    vTaskDelete(NULL);
}

void test_task_delay_zero_yield_order(void) {
    s_yield_trace_idx = 0;
    memset(s_yield_trace, 0, sizeof(s_yield_trace));

    TaskHandle_t h1, h2;
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(yield_task1, "y1", 32 * 1024, NULL, 5, &h1));
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(yield_task2, "y2", 32 * 1024, NULL, 5, &h2));

    wink_status_t st = pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 50);
    TEST_ASSERT_EQUAL(WINK_OK, st);

    TEST_ASSERT_EQUAL(3, s_yield_trace_idx);
    TEST_ASSERT_EQUAL(1, s_yield_trace[0]);
    TEST_ASSERT_EQUAL(2, s_yield_trace[1]);
    TEST_ASSERT_EQUAL(3, s_yield_trace[2]);
}

/* --------------------------------------------------------------------------
 * 5. Delay and DelayUntil Accuracy
 * -------------------------------------------------------------------------- */
static TickType_t s_delay_measured = 0;
static TickType_t s_delay_until_measured = 0;

static void delay_test_task(void* arg) {
    (void)arg;
    TickType_t t0 = xTaskGetTickCount();
    vTaskDelay(5);
    TickType_t t1 = xTaskGetTickCount();
    s_delay_measured = t1 - t0;

    TickType_t prev = xTaskGetTickCount();
    vTaskDelayUntil(&prev, 8);
    TickType_t t2 = xTaskGetTickCount();
    s_delay_until_measured = t2 - t1;

    vTaskDelete(NULL);
}

void test_task_delay_and_delay_until(void) {
    s_delay_measured = 0;
    s_delay_until_measured = 0;

    TaskHandle_t h;
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(delay_test_task, "delay_t", 32 * 1024, NULL, 5, &h));

    wink_status_t st = pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 50);
    TEST_ASSERT_EQUAL(WINK_OK, st);

    TEST_ASSERT_GREATER_OR_EQUAL(5, s_delay_measured);
    TEST_ASSERT_GREATER_OR_EQUAL(8, s_delay_until_measured);
}

/* --------------------------------------------------------------------------
 * 6. Queue FIFO, Timeout, and Dual-Waiters Isolation
 * -------------------------------------------------------------------------- */
static QueueHandle_t s_q = NULL;
static int s_rx_items[3];
static int s_rx_idx = 0;
static bool s_timeout_ok = false;

static void queue_reader_task(void* arg) {
    (void)arg;
    int val = 0;
    /* Receive 2 items that are sent by main */
    if (xQueueReceive(s_q, &val, 10) == pdPASS) {
        s_rx_items[s_rx_idx++] = val;
    }
    if (xQueueReceive(s_q, &val, 10) == pdPASS) {
        s_rx_items[s_rx_idx++] = val;
    }
    /* Third receive should timeout */
    if (xQueueReceive(s_q, &val, 5) == errQUEUE_EMPTY) {
        s_timeout_ok = true;
    }
    vTaskDelete(NULL);
}

static void queue_writer_task(void* arg) {
    (void)arg;
    int v1 = 100;
    int v2 = 200;
    xQueueSend(s_q, &v1, 0);
    xQueueSend(s_q, &v2, 0);
    vTaskDelete(NULL);
}

void test_queue_fifo_and_timeout(void) {
    s_q = xQueueCreate(2, sizeof(int));
    TEST_ASSERT_NOT_NULL(s_q);
    s_rx_idx = 0;
    s_timeout_ok = false;

    TaskHandle_t hr, hw;
    /* Reader created first: blocks on empty queue */
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(queue_reader_task, "qr", 32 * 1024, NULL, 5, &hr));
    /* Writer created second: writes items and wakes reader */
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(queue_writer_task, "qw", 32 * 1024, NULL, 5, &hw));

    wink_status_t st = pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 50);
    TEST_ASSERT_EQUAL(WINK_OK, st);

    TEST_ASSERT_EQUAL(2, s_rx_idx);
    TEST_ASSERT_EQUAL(100, s_rx_items[0]);
    TEST_ASSERT_EQUAL(200, s_rx_items[1]);
    TEST_ASSERT_TRUE(s_timeout_ok);

    vQueueDelete(s_q);
}

/* --------------------------------------------------------------------------
 * 7. Mutex Priority-One Waking Order
 * -------------------------------------------------------------------------- */
static SemaphoreHandle_t s_mtx = NULL;
static int s_mtx_order[3];
static int s_mtx_order_idx = 0;

static void mtx_holder_task(void* arg) {
    (void)arg;
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    /* Delay 2 ticks so contenders have both run and blocked */
    vTaskDelay(2);
    xSemaphoreGive(s_mtx);
    vTaskDelete(NULL);
}

static void mtx_contender_low(void* arg) {
    (void)arg;
    /* Block until mutex available */
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    s_mtx_order[s_mtx_order_idx++] = 1; /* Low prio */
    xSemaphoreGive(s_mtx);
    vTaskDelete(NULL);
}

static void mtx_contender_high(void* arg) {
    (void)arg;
    /* Block until mutex available */
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    s_mtx_order[s_mtx_order_idx++] = 2; /* High prio */
    xSemaphoreGive(s_mtx);
    vTaskDelete(NULL);
}

void test_mutex_priority_waking_order(void) {
    s_mtx = xSemaphoreCreateMutex();
    TEST_ASSERT_NOT_NULL(s_mtx);
    s_mtx_order_idx = 0;
    memset(s_mtx_order, 0, sizeof(s_mtx_order));

    TaskHandle_t hHolder, hLow, hHigh;
    /* Holder created first with prio 5 */
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(mtx_holder_task, "m_h", 32 * 1024, NULL, 5, &hHolder));
    /* Low contender created next with prio 2 */
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(mtx_contender_low, "m_l", 32 * 1024, NULL, 2, &hLow));
    /* High contender created next with prio 10 */
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(mtx_contender_high, "m_hi", 32 * 1024, NULL, 10, &hHigh));

    wink_status_t st = pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 50);
    TEST_ASSERT_EQUAL(WINK_OK, st);

    TEST_ASSERT_EQUAL(2, s_mtx_order_idx);
    /* High priority contender (2) must acquire before Low priority (1) */
    TEST_ASSERT_EQUAL(2, s_mtx_order[0]);
    TEST_ASSERT_EQUAL(1, s_mtx_order[1]);

    vSemaphoreDelete(s_mtx);
}

/* --------------------------------------------------------------------------
 * 8. EventGroup Broadcast & ClearOnExit
 * -------------------------------------------------------------------------- */
static EventGroupHandle_t s_eg = NULL;
static bool s_eg_waiter1_done = false;
static bool s_eg_waiter2_done = false;

static void eg_waiter1(void* arg) {
    (void)arg;
    /* Wait for bit 0x01, clear on exit */
    EventBits_t bits = xEventGroupWaitBits(s_eg, 0x01, pdTRUE, pdFALSE, 20);
    if ((bits & 0x01) != 0) {
        s_eg_waiter1_done = true;
    }
    vTaskDelete(NULL);
}

static void eg_waiter2(void* arg) {
    (void)arg;
    /* Wait for both 0x01 AND 0x02, do NOT clear on exit */
    EventBits_t bits = xEventGroupWaitBits(s_eg, 0x03, pdFALSE, pdTRUE, 20);
    if ((bits & 0x03) == 0x03) {
        s_eg_waiter2_done = true;
    }
    vTaskDelete(NULL);
}

static void eg_setter(void* arg) {
    (void)arg;
    vTaskDelay(1);
    /* Set only bit 0x01: waiter1 should wake, clear bit 0x01 */
    xEventGroupSetBits(s_eg, 0x01);
    vTaskDelay(2);
    /* Now set both 0x01 and 0x02: waiter2 should wake */
    xEventGroupSetBits(s_eg, 0x03);
    vTaskDelete(NULL);
}

void test_event_group_broadcast_and_clear_on_exit(void) {
    s_eg = xEventGroupCreate();
    TEST_ASSERT_NOT_NULL(s_eg);
    s_eg_waiter1_done = false;
    s_eg_waiter2_done = false;

    TaskHandle_t h1, h2, hs;
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(eg_waiter1, "eg1", 32 * 1024, NULL, 5, &h1));
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(eg_waiter2, "eg2", 32 * 1024, NULL, 5, &h2));
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(eg_setter, "egs", 32 * 1024, NULL, 5, &hs));

    wink_status_t st = pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 50);
    TEST_ASSERT_EQUAL(WINK_OK, st);

    TEST_ASSERT_TRUE(s_eg_waiter1_done);
    TEST_ASSERT_TRUE(s_eg_waiter2_done);

    vEventGroupDelete(s_eg);
}

static bool s_mw1_done = false;
static bool s_mw2_done = false;

static void mw_waiter1(void* arg) {
    (void)arg;
    EventBits_t bits = xEventGroupWaitBits(s_eg, 0x04, pdTRUE, pdFALSE, 20);
    if ((bits & 0x04) != 0) {
        s_mw1_done = true;
    }
    vTaskDelete(NULL);
}

static void mw_waiter2(void* arg) {
    (void)arg;
    EventBits_t bits = xEventGroupWaitBits(s_eg, 0x04, pdTRUE, pdFALSE, 20);
    if ((bits & 0x04) != 0) {
        s_mw2_done = true;
    }
    vTaskDelete(NULL);
}

static void mw_setter(void* arg) {
    (void)arg;
    vTaskDelay(1);
    xEventGroupSetBits(s_eg, 0x04);
    vTaskDelete(NULL);
}

void test_event_group_multi_waiter_same_bit_clear_on_exit(void) {
    s_eg = xEventGroupCreate();
    TEST_ASSERT_NOT_NULL(s_eg);
    s_mw1_done = false;
    s_mw2_done = false;

    TaskHandle_t h1, h2, hs;
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(mw_waiter1, "mw1", 32 * 1024, NULL, 5, &h1));
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(mw_waiter2, "mw2", 32 * 1024, NULL, 5, &h2));
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(mw_setter, "mws", 32 * 1024, NULL, 5, &hs));

    wink_status_t st = pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 50);
    TEST_ASSERT_EQUAL(WINK_OK, st);

    /* Both waiters must have observed 0x04 despite xClearOnExit == pdTRUE */
    TEST_ASSERT_TRUE(s_mw1_done);
    TEST_ASSERT_TRUE(s_mw2_done);

    /* After both exited, 0x04 must be cleared */
    TEST_ASSERT_EQUAL(0, xEventGroupGetBits(s_eg) & 0x04);

    vEventGroupDelete(s_eg);
}

/* --------------------------------------------------------------------------
 * 9. Dual Task 200ms / 500ms Alternation (M1 DoD explicitly specified)
 * -------------------------------------------------------------------------- */
static int s_alt_trace[12];
static int s_alt_count = 0;

static void task_200ms(void* arg) {
    (void)arg;
    for (int i = 0; i < 4; ++i) {
        s_alt_trace[s_alt_count++] = 200;
        vTaskDelay(20); /* 20 ticks = 200ms */
    }
    vTaskDelete(NULL);
}

static void task_500ms(void* arg) {
    (void)arg;
    for (int i = 0; i < 2; ++i) {
        s_alt_trace[s_alt_count++] = 500;
        vTaskDelay(50); /* 50 ticks = 500ms */
    }
    vTaskDelete(NULL);
}

void test_dual_task_200ms_500ms_alternation(void) {
    s_alt_count = 0;
    memset(s_alt_trace, 0, sizeof(s_alt_trace));

    TaskHandle_t h1, h2;
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(task_200ms, "t200", 32 * 1024, NULL, 5, &h1));
    TEST_ASSERT_EQUAL(pdPASS, xTaskCreate(task_500ms, "t500", 32 * 1024, NULL, 5, &h2));

    wink_status_t st = pal_sim_scheduler_run(NULL, SIM_SCHED_NO_READY, 120);
    TEST_ASSERT_EQUAL(WINK_OK, st);

    TEST_ASSERT_EQUAL(6, s_alt_count);
    TEST_ASSERT_EQUAL(200, s_alt_trace[0]);
    TEST_ASSERT_EQUAL(500, s_alt_trace[1]);
    TEST_ASSERT_EQUAL(200, s_alt_trace[2]);
    TEST_ASSERT_EQUAL(200, s_alt_trace[3]);
    TEST_ASSERT_EQUAL(500, s_alt_trace[4]);
    TEST_ASSERT_EQUAL(200, s_alt_trace[5]);
}

/* --------------------------------------------------------------------------
 * 10. FromISR & Stubs Fail-Loud
 * -------------------------------------------------------------------------- */
void test_from_isr_and_stubs(void) {
    QueueHandle_t q = xQueueCreate(2, sizeof(int));
    int item = 42;
    BaseType_t woken = pdTRUE;
    TEST_ASSERT_EQUAL(pdPASS, xQueueSendFromISR(q, &item, &woken));
    TEST_ASSERT_EQUAL(pdFALSE, woken);

    /* Test xQueueSendToBack and xQueueSendToBackFromISR macros */
    int item2 = 84;
    woken = pdTRUE;
    TEST_ASSERT_EQUAL(pdPASS, xQueueSendToBackFromISR(q, &item2, &woken));
    TEST_ASSERT_EQUAL(pdFALSE, woken);

    /* Test xQueuePeekFromISR */
    int peek_item = 0;
    TEST_ASSERT_EQUAL(pdPASS, xQueuePeekFromISR(q, &peek_item));
    TEST_ASSERT_EQUAL(42, peek_item);

    int out_item = 0;
    woken = pdTRUE;
    TEST_ASSERT_EQUAL(pdPASS, xQueueReceiveFromISR(q, &out_item, &woken));
    TEST_ASSERT_EQUAL(pdFALSE, woken);
    TEST_ASSERT_EQUAL(42, out_item);

    /* Test xQueueSendToBack macro in task context */
    int item3 = 100;
    TEST_ASSERT_EQUAL(pdPASS, xQueueSendToBack(q, &item3, 0));

    /* Test Fail-Loud stubs for xQueueSendToFront / ISR */
    int item_front = 999;
    woken = pdTRUE;
    TEST_ASSERT_EQUAL(errQUEUE_FULL, xQueueSendToFront(q, &item_front, 0));
    TEST_ASSERT_EQUAL(errQUEUE_FULL, xQueueSendToFrontFromISR(q, &item_front, &woken));
    TEST_ASSERT_EQUAL(pdFALSE, woken);

    vQueueDelete(q);

    /* Timers Fail-Loud */
    TEST_ASSERT_NULL(xTimerCreate("t", 10, pdTRUE, NULL, NULL));
    TEST_ASSERT_EQUAL(pdFAIL, xTimerStart(NULL, 0));
    TEST_ASSERT_EQUAL(pdFAIL, xTimerStop(NULL, 0));

    /* Recursive mutex Fail-Loud */
    TEST_ASSERT_NULL(xSemaphoreCreateRecursiveMutex());
    TEST_ASSERT_EQUAL(pdFAIL, xSemaphoreTakeRecursive(NULL, 0));
    TEST_ASSERT_EQUAL(pdFAIL, xSemaphoreGiveRecursive(NULL));

    /* Miscellaneous FreeRTOS APIs */
    TEST_ASSERT_EQUAL(0, xPortGetCoreID());
    TEST_ASSERT_EQUAL(taskSCHEDULER_RUNNING, xTaskGetSchedulerState());
    TEST_ASSERT_EQUAL(UINT32_MAX, uxTaskGetStackHighWaterMark(NULL));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_task_handle_aba_protection);
    RUN_TEST(test_task_null_handle_and_self_delete);
    RUN_TEST(test_task_delay_zero_yield_order);
    RUN_TEST(test_task_delay_and_delay_until);
    RUN_TEST(test_queue_fifo_and_timeout);
    RUN_TEST(test_mutex_priority_waking_order);
    RUN_TEST(test_event_group_broadcast_and_clear_on_exit);
    RUN_TEST(test_event_group_multi_waiter_same_bit_clear_on_exit);
    RUN_TEST(test_dual_task_200ms_500ms_alternation);
    RUN_TEST(test_from_isr_and_stubs);
    return UNITY_END();
}
