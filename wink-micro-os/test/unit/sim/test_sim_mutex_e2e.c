// SPDX-License-Identifier: Apache-2.0
/**
 * @file test_sim_mutex_e2e.c
 * @brief E2E unit tests for mutex BLOCKED path under wasm/host cooperative scheduler.
 */
#include "unity.h"
#include "pal_osal.h"
#include "wink_sim_scheduler.h"
#include <stdint.h>
#include <string.h>

#include "compat/wink_test_compat.h"
WINK_TEST_ALLOW_DEPRECATED

static pal_os_mutex_t g_mtx;
static volatile uint32_t g_owner_order[4];
static volatile uint32_t g_owner_count;
static volatile uint32_t g_critical_section_guarded;

void setUp(void) {
    sim_scheduler_reset(0xBADF00D);
    g_mtx = pal_os_mutex_create();
    TEST_ASSERT_NOT_NULL(g_mtx);
    g_owner_count = 0;
    memset((void*)g_owner_order, 0, sizeof(g_owner_order));
    g_critical_section_guarded = 0;
}

void tearDown(void) {
    pal_os_mutex_destroy(g_mtx);
    g_mtx = NULL;
    sim_scheduler_reset(0);
}

void task_no_contention(void* arg) {
    (void)arg;
    TEST_ASSERT_EQUAL(WINK_OK, pal_os_mutex_lock(g_mtx, WINK_MUTEX_WAIT_FOREVER));
    g_critical_section_guarded = 1;
    pal_os_sleep_ms(1);
    g_critical_section_guarded = 0;
    TEST_ASSERT_EQUAL(WINK_OK, pal_os_mutex_unlock(g_mtx));
    pal_os_task_delete(NULL);
}

void test_mutex_no_contention(void) {
    pal_os_task_handle_t h;
    TEST_ASSERT_EQUAL(WINK_OK,
        pal_os_task_create(task_no_contention, "nocont", 16*1024, NULL, 5, PAL_OS_CORE_ANY, &h));
    TEST_ASSERT_EQUAL(WINK_OK, pal_sim_scheduler_run(NULL, 0, 200));
    TEST_ASSERT_EQUAL_UINT32(0, g_critical_section_guarded);
}

static void contender_task(void* arg) {
    uint32_t id = (uint32_t)(uintptr_t)arg;
    TEST_ASSERT_EQUAL(WINK_OK, pal_os_mutex_lock(g_mtx, WINK_MUTEX_WAIT_FOREVER));
    uint32_t idx = g_owner_count++;
    TEST_ASSERT(idx < 4);
    g_owner_order[idx] = id;
    TEST_ASSERT_EQUAL_UINT32(0, g_critical_section_guarded);
    g_critical_section_guarded = id + 1;
    pal_os_sleep_ms(1);
    g_critical_section_guarded = 0;
    TEST_ASSERT_EQUAL(WINK_OK, pal_os_mutex_unlock(g_mtx));
    pal_os_task_delete(NULL);
}

void task_fifo_holder(void* arg) {
    (void)arg;
    TEST_ASSERT_EQUAL(WINK_OK, pal_os_mutex_lock(g_mtx, WINK_MUTEX_WAIT_FOREVER));
    for (uint32_t i = 0; i < 3; ++i) {
        char name[8] = { 'c', (char)('0'+i), 0 };
        pal_os_task_handle_t h;
        TEST_ASSERT_EQUAL(WINK_OK,
            pal_os_task_create(contender_task, name, 16*1024, (void*)(uintptr_t)i, 4, PAL_OS_CORE_ANY, &h));
    }
    pal_os_sleep_ms(1);
    TEST_ASSERT_EQUAL_UINT32(0, g_owner_count);
    TEST_ASSERT_EQUAL(WINK_OK, pal_os_mutex_unlock(g_mtx));
    pal_os_sleep_ms(10);
    pal_os_task_delete(NULL);
}

void test_mutex_fifo_contention(void) {
    pal_os_task_handle_t h;
    TEST_ASSERT_EQUAL(WINK_OK,
        pal_os_task_create(task_fifo_holder, "holder", 16*1024, NULL, 5, PAL_OS_CORE_ANY, &h));
    TEST_ASSERT_EQUAL(WINK_OK, pal_sim_scheduler_run(NULL, 0, 500));
    TEST_ASSERT_EQUAL_UINT32(3, g_owner_count);
    TEST_ASSERT_EQUAL_UINT32(0, g_owner_order[0]);
    TEST_ASSERT_EQUAL_UINT32(1, g_owner_order[1]);
    TEST_ASSERT_EQUAL_UINT32(2, g_owner_order[2]);
}

void task_lock_holder_short(void* arg) {
    (void)arg;
    TEST_ASSERT_EQUAL(WINK_OK, pal_os_mutex_lock(g_mtx, WINK_MUTEX_WAIT_FOREVER));
    pal_os_sleep_ms(10);
    TEST_ASSERT_EQUAL(WINK_OK, pal_os_mutex_unlock(g_mtx));
    pal_os_task_delete(NULL);
}

void task_timeout_try(void* arg) {
    volatile wink_status_t* rs = (volatile wink_status_t*)arg;
    pal_os_sleep_ms(1);
    *rs = pal_os_mutex_lock(g_mtx, 1);
    pal_os_task_delete(NULL);
}

void test_mutex_timeout_returns_timeout(void) {
    volatile wink_status_t try_result = WINK_OK;
    pal_os_task_handle_t h1, h2;
    TEST_ASSERT_EQUAL(WINK_OK,
        pal_os_task_create(task_lock_holder_short, "holder", 16*1024, NULL, 5, PAL_OS_CORE_ANY, &h1));
    TEST_ASSERT_EQUAL(WINK_OK,
        pal_os_task_create(task_timeout_try, "tryr", 16*1024, (void*)&try_result, 5, PAL_OS_CORE_ANY, &h2));
    TEST_ASSERT_EQUAL(WINK_OK, pal_sim_scheduler_run(NULL, 0, 500));
    TEST_ASSERT_EQUAL(WINK_ERR_TIMEOUT, try_result);
}

void task_wrong_unlocker(void* arg) {
    (void)arg;
    TEST_ASSERT_EQUAL(WINK_ERR_INVALID_STATE, pal_os_mutex_unlock(g_mtx));
    pal_os_task_delete(NULL);
}

void test_mutex_non_owner_unlock_rejected(void) {
    pal_os_task_handle_t h;
    TEST_ASSERT_EQUAL(WINK_OK,
        pal_os_task_create(task_wrong_unlocker, "wrongu", 16*1024, NULL, 5, PAL_OS_CORE_ANY, &h));
    TEST_ASSERT_EQUAL(WINK_OK, pal_sim_scheduler_run(NULL, 0, 100));
}

void test_mutex_null_handle_invalid_arg(void) {
    TEST_ASSERT_EQUAL(WINK_ERR_INVALID_ARG, pal_os_mutex_lock(NULL, WINK_MUTEX_WAIT_FOREVER));
    TEST_ASSERT_EQUAL(WINK_ERR_INVALID_ARG, pal_os_mutex_unlock(NULL));
    pal_os_mutex_destroy(NULL);
}

void task_recursive_try(void* arg) {
    (void)arg;
    TEST_ASSERT_EQUAL(WINK_OK, pal_os_mutex_lock(g_mtx, WINK_MUTEX_WAIT_FOREVER));
    TEST_ASSERT_EQUAL(WINK_ERR_BUSY, pal_os_mutex_lock(g_mtx, WINK_MUTEX_WAIT_FOREVER));
    TEST_ASSERT_EQUAL(WINK_OK, pal_os_mutex_unlock(g_mtx));
    pal_os_task_delete(NULL);
}

void test_mutex_recursive_relock_rejected(void) {
    pal_os_task_handle_t h;
    TEST_ASSERT_EQUAL(WINK_OK,
        pal_os_task_create(task_recursive_try, "recur", 16*1024, NULL, 5, PAL_OS_CORE_ANY, &h));
    TEST_ASSERT_EQUAL(WINK_OK, pal_sim_scheduler_run(NULL, 0, 200));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_mutex_no_contention);
    RUN_TEST(test_mutex_fifo_contention);
    RUN_TEST(test_mutex_timeout_returns_timeout);
    RUN_TEST(test_mutex_non_owner_unlock_rejected);
    RUN_TEST(test_mutex_null_handle_invalid_arg);
    RUN_TEST(test_mutex_recursive_relock_rejected);
    return UNITY_END();
}
